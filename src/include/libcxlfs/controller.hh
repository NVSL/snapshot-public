// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   controller.hh
 * @date   novembre 16, 2022
 * @brief  Brief description here
 */

#include <functional>

#include "libcxlfs/pfmonitor.hh"
#include "libcxlfs/membwdist.hh"
#include "libcxlfs/blkdev.hh"

class Controller {
public:
	using addr_t = uint64_t;
	using heat_t = bool;
  	using dist_t = heat_t *;
private:
  
	UserBlkDev *ubd;
	PFMonitor *pfm;
	MemBWDist mbd;
	bool run_out_mapped_page;

	uint64_t page_size;
    uint64_t page_count;
	uint64_t page_use;
	

	// char *shared_mem;

	addr_t *shared_mem_start;

	addr_t *shared_mem_start_end;

	addr_t* get_avaible_page()
	void notify_page_fault(addr_t *addr);
	addr_t* evict_a_page(addr_t start, addr_t end);
	int map_new_page_from_blkdev(addr_t *addr);

	
public:
  Controller() {}
  /** @brief Initialize the internal state **/
  int init();

};