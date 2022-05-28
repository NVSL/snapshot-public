#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "nvsl/clock.hh"
#include "nvsl/common.hh"
#include "nvsl/error.hh"
#include "nvsl/string.hh"
#include "nvsl/utils.hh"

#include "famus.h"

#ifdef RELEASE
#define DISABLE_IF_RELEASE(expr)
#else
#define DISABLE_IF_RELEASE(expr) \
  if (true) {                    \
    expr;                        \
  }
#endif

static inline int rwperm(mode_t m, unsigned int r, unsigned int w) {
  return (!!(m & S_IRUSR) == r) && (!!(m & S_IWUSR) == w);
}

/**
 * @note From: Terence Kelly, "Good Old-Fashioned Persistent Memory", USENIX
 * ;login; Winter 2019, Vol. 44, No. 4.
 *
 * We must fsync() backing file twice to ensure that snapshot data are durable
 * before success indicator (file permission) becomes durable.  We're not using
 * fallocate() to reserve space for worst-case scenario in which backing file
 * and snapshot file diverge completely, because that could defeat the reflink
 * sharing that makes snapshots efficient; read "man ioctl_ficlone".  The
 * "ioctl(FICLONE)" works only on reflink-enabled file systems, e.g., BtrFS,
 * XFS, OCFS2. */
int snap_fd = -1;
// int dir_fd = -1;
int famus_snap_sync(int bfd) {
  struct stat sb;

  if (snap_fd == -1) {
    const std::string bfname = nvsl::fd_to_fname(bfd);
    const std::string snapshot_file = bfname + ".snapshot";

    std::cerr << "snapshot file = " << snapshot_file << "\n";

    snap_fd = open(snapshot_file.c_str(), O_CREAT | O_RDWR, 0666);
    if (snap_fd == -1) {
      DBGE << "Unable to open fd for the snapshot file " << snapshot_file
           << "\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }
  }

  auto fatal_if = [&](bool cond, std::string fname, int line) {
    if (cond) {
      DBGE << "Failed at " << fname << ":" << line << "\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }
  };

  nvsl::Clock clk;

  DISABLE_IF_RELEASE(clk.tick());
  fatal_if((0 != fstat(snap_fd, &sb)), __FILE__, __LINE__);
  DISABLE_IF_RELEASE(clk.tock());
  DISABLE_IF_RELEASE(std::cerr << "fstat took = " << clk.ns() / 1000.0
                               << " us\n");
  // fatal_if((!rwperm(sb.st_mode, 0, 1)), __FILE__, __LINE__);

  clk.reset();
  DISABLE_IF_RELEASE(clk.tick());
  fatal_if((0 != ioctl(snap_fd, FICLONE, bfd)), __FILE__, __LINE__);
  DISABLE_IF_RELEASE(clk.tock());
  DISABLE_IF_RELEASE(std::cerr << "ioctl took = " << clk.ns() / 1000.0
                               << " us\n");

  clk.reset();
  DISABLE_IF_RELEASE(clk.tick());
  fatal_if((0 != fsync(snap_fd)), __FILE__, __LINE__);
  DISABLE_IF_RELEASE(clk.tock());
  DISABLE_IF_RELEASE(std::cerr << "fsync took = " << clk.ns() / 1000.0
                               << " us\n");

  clk.reset();
  DISABLE_IF_RELEASE(clk.tick());
  fatal_if((0 != fchmod(snap_fd, S_IRUSR)), __FILE__, __LINE__);
  DISABLE_IF_RELEASE(clk.tock());
  DISABLE_IF_RELEASE(std::cerr << "fchmod took = " << clk.ns() / 1000.0
                               << " us\n");

  clk.reset();
  DISABLE_IF_RELEASE(clk.tick());
  fatal_if((0 != fstat(snap_fd, &sb)), __FILE__, __LINE__);
  DISABLE_IF_RELEASE(clk.tock());
  DISABLE_IF_RELEASE(std::cerr << "fstat took = " << clk.ns() / 1000.0
                               << " us\n");

  clk.reset();
  DISABLE_IF_RELEASE(clk.tick());
  fatal_if((!rwperm(sb.st_mode, 1, 0)), __FILE__, __LINE__);
  fatal_if((0 != fsync(snap_fd)), __FILE__, __LINE__);
  fatal_if((0 != lseek(snap_fd, 0, SEEK_SET)), __FILE__, __LINE__);
  // fatal_if((0 < dir_fd && 0 != fsync(dirfd)), __FILE__, __LINE__);
  // fatal_if((0 != close(snap_fd)), __FILE__, __LINE__);
  return 0;
}
