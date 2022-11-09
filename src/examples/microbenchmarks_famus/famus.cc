#include <dirent.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <filesystem>

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
int snap_fd0 = -1, snap_fd1 = -1;
int fd_switch = 0;
size_t msync_cnt = 0;

int famus_snap_sync(int bfd, void *bf_mem, size_t bf_sz) {
  struct stat sb;

  auto fatal_if = [&](bool cond, std::string fname, int line) {
    if (cond) {
      DBGE << "Failed at " << fname << ":" << line << "\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }
  };

  if (snap_fd0 == -1) {
    const std::string bfname = nvsl::fd_to_fname(bfd);
    const std::string snapshot0_file = bfname + ".snapshot0";
    const std::string snapshot1_file = bfname + ".snapshot1";

    snap_fd0 = open(snapshot0_file.c_str(), O_CREAT | O_RDWR, S_IWUSR);
    if (snap_fd0 == -1) {
      DBGE << "Unable to open fd for the snapshot file " << snapshot0_file
           << "\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }

    snap_fd1 = open(snapshot1_file.c_str(), O_CREAT | O_RDWR, S_IWUSR);
    if (snap_fd1 == -1) {
      DBGE << "Unable to open fd for the snapshot file " << snapshot1_file
           << "\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }

    int dir_fd =
        open(std::filesystem::path(bfname).parent_path().c_str(), O_RDONLY);
    fatal_if(dir_fd == -1, __FILE__, __LINE__);
    fatal_if((fsync(dir_fd) == -1), __FILE__, __LINE__);
  }

  fatal_if(0 != msync(bf_mem, bf_sz, MS_SYNC), __FILE__, __LINE__);

  int snap_fd = fd_switch ? snap_fd0 : snap_fd1;

  nvsl::Clock clk;

  fatal_if((-1 == fsync(snap_fd)), __FILE__, __LINE__);

  fatal_if((0 != fstat(snap_fd, &sb)), __FILE__, __LINE__);
  // fatal_if((!rwperm(sb.st_mode, 0, 1)), __FILE__, __LINE__);

  clk.reset();
  clk.tick();
  fatal_if((0 != ioctl(snap_fd, FICLONE, bfd)), __FILE__, __LINE__);

  const std::string cmd_snapshot =
      "xfs_bmap /mnt/pmem0p4/microbench.0.snapshot0 > /tmp/xfs_bmap.snapshot0" +
      nvsl::S(msync_cnt) + ".txt";
  const std::string cmd_backing =
      "xfs_bmap /mnt/pmem0p4/microbench.0 > /tmp/xfs_bmap.backing" +
      nvsl::S(msync_cnt) + ".txt";
  msync_cnt++;
  fatal_if(0 != system(cmd_snapshot.c_str()), __FILE__, __LINE__);
  fatal_if(0 != system(cmd_backing.c_str()), __FILE__, __LINE__);

  clk.tock();
  std::cout << clk.ns() << "\n";

  fatal_if((0 != fsync(snap_fd)), __FILE__, __LINE__);
  fatal_if((0 != fchmod(snap_fd, S_IRUSR)), __FILE__, __LINE__);
  fatal_if((0 != fstat(snap_fd, &sb)), __FILE__, __LINE__);
  fatal_if((!rwperm(sb.st_mode, 1, 0)), __FILE__, __LINE__);
  fatal_if((0 != fsync(snap_fd)), __FILE__, __LINE__);
  fatal_if((0 != lseek(snap_fd, 0, SEEK_SET)), __FILE__, __LINE__);

  fd_switch = not fd_switch;

  return 0;
}
