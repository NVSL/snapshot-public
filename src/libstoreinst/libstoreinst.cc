#include <cassert>
#include <fcntl.h>
#include <immintrin.h>
#include <iostream>
#include <numeric>
#include <sys/mman.h>

#include "bgflush.hh"
#include "common.hh"
#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "libvram/libvram.hh"
#include "log.hh"
#include "nvsl/clock.hh"
#include "nvsl/common.hh"
#include "nvsl/envvars.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/stats.hh"
#include "nvsl/trace.hh"

NVSL_DECL_ENV(CXLBUF_CRASH_ON_COMMIT);
NVSL_DECL_ENV(ENABLE_CHECK_MEMORY_TRACING);
NVSL_DECL_ENV(PMEM_END_ADDR);
NVSL_DECL_ENV(PMEM_START_ADDR);
NVSL_DECL_ENV(CXLBUF_MSYNC_IS_NOP);
NVSL_DECL_ENV(CXLBUF_MSYNC_SLEEP_NS);
NVSL_DECL_ENV(CXLBUF_LOG_LOC);

#define TRACE_FILE "/tmp/cxlbuf.trace"

bool firstSnapshot = true;
bool crashOnCommit = false;
bool nopMsync = false;
size_t msyncSleepNs = 0;
int trace_fd = -1;

namespace nvsl {
  namespace cxlbuf {
    std::string *log_loc;
    void *backing_file_start;
  } // namespace cxlbuf
} // namespace nvsl

extern "C" {
void *start_addr, *end_addr = nullptr;
bool startTracking = false;
bool storeInstEnabled = false;
bool cxlModeEnabled = false;
nvsl::PMemOps *pmemops;
std::ofstream *traceStream;

void init_counters() {
  nvsl::cxlbuf::skip_check_count = new nvsl::Counter();
  nvsl::cxlbuf::logged_check_count = new nvsl::Counter();
  nvsl::cxlbuf::tx_log_count_dist = new nvsl::StatsFreq<>();

  nvsl::cxlbuf::skip_check_count->init("skip_check_count",
                                       "Skipped memory checks");
  nvsl::cxlbuf::logged_check_count->init("logged_check_count",
                                         "Logged memory checks");
  nvsl::cxlbuf::tx_log_count_dist->init(
      "tx_log_count_dist", "Distribution of number of logs in a transaction", 5,
      0, 30);
  perst_overhead_clk = new nvsl::Clock();
}

void init_addrs() {
  const auto start_addr_str = get_env_str(PMEM_START_ADDR_ENV);
  if (start_addr_str == "") {
    DBGE << "PMEM_START_ADDR not set" << std::endl;
    exit(1);
  }

  const auto end_addr_str = get_env_str(PMEM_END_ADDR_ENV);
  if (end_addr_str == "") {
    DBGE << "PMEM_END_ADDR not set" << std::endl;
    exit(1);
  }

  start_addr = (void *)std::stoull(start_addr_str, 0, 16);
  end_addr = (void *)std::stoull(end_addr_str, 0, 16);

  assert(start_addr != nullptr);
  assert(end_addr != nullptr);

  assert(start_addr < end_addr);
}

void init_pmemops() {
  pmemops = new nvsl::PMemOpsClwb();
}

void init_envvars() {
  crashOnCommit = get_env_val(CXLBUF_CRASH_ON_COMMIT_ENV);
  nopMsync = get_env_val(CXLBUF_MSYNC_IS_NOP_ENV);
  nvsl::cxlbuf::log_loc = new std::string(
      get_env_str(CXLBUF_LOG_LOC_ENV, "/mnt/pmem0/cxlbuf_logs/"));

  const auto msyncSleepNsStr = get_env_str(CXLBUF_MSYNC_SLEEP_NS_ENV);
  try {
    msyncSleepNs = std::stod(msyncSleepNsStr);
  } catch (const std::exception &e) {
    msyncSleepNs = 0;
  }

  std::cerr << "nopMsync = " << nopMsync << std::endl;
  std::cerr << "msyncSleepNS = " << msyncSleepNs << std::endl;
}

void init_vram() {
  ctor_libvram();
}

/**
 * @brief Constructor for the libstoreinst shared object
 * @detail Should run first since it defines memcpy and other important stuff
 */
__attribute__((__constructor__(101))) void libstoreinst_ctor() {
  nvsl::cxlbuf::init_dlsyms();
  init_counters();
  init_addrs();
  init_pmemops();
  init_envvars();

#ifdef ENABLE_LIBVRAM
  init_vram();
#endif

#ifdef TRACE_LOG_MSYNC
  trace_fd = open(TRACE_FILE, O_CREAT | O_TRUNC | O_RDWR, 0666);
  if (trace_fd == -1) {
    fprintf(stderr, "Unable to open trace file");
    exit(1);
  } else {
  }
#endif

  traceStream = new std::ofstream("/tmp/stacktrace");
  cxlModeEnabled = get_env_val("CXL_MODE_ENABLED");

  DBGH(1) << "Address range = [" << start_addr << ", " << end_addr << "]"
          << std::endl;

#ifdef USE_BGFLUSH
  nvsl::cxlbuf::bgflush::launch();
#endif
}

__attribute__((unused)) void checkMemory(void *ptr) {
  // std::cerr << "CheckMemory at " << ptr << std::endl;

#ifdef NO_CHECK_MEMORY
  return;
#endif

  if (startTracking) {
    if (start_addr <= ptr and ptr < end_addr) {
      local_log.log_range(ptr, 8);
#ifndef RELEASE
      if (get_env_val(ENABLE_CHECK_MEMORY_TRACING_ENV)) {
        *traceStream << "-----\n" << nvsl::get_stack_trace() << "\n\n";
      }
#endif
    } else {
#ifdef CXLBUF_TESTING_GOODIES
      ++*nvsl::cxlbuf::skip_check_count;
#endif
    }
  }
}

__attribute__((destructor)) void libstoreinst_dtor() {
  perst_overhead_clk->reconcile();

  std::cerr << "Summary:\n";
  std::cerr << "snapshots = " << snapshots.value() << std::endl;
  std::cerr << nvsl::cxlbuf::skip_check_count->str() << "\n";
  std::cerr << nvsl::cxlbuf::logged_check_count->str() << "\n";
  std::cerr << nvsl::cxlbuf::tx_log_count_dist->str() << "\n";

  std::cerr << "perst_overhead = " << perst_overhead_clk->ns() << std::endl;
}
}
