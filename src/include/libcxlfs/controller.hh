// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   controller.hh
 * @date   novembre 16, 2022
 * @brief  Brief description here
 */
#pragma once
#include <functional>

#include "libcxlfs/pfmonitor.hh"
#include "libcxlfs/membwdist.hh"
#include "libcxlfs/blkdev.hh"

class Controller {
public:
	
private:
  
	UserBlkDev *ubd;
	PFMonitor *pfm;
	MemBWDist *mbd;
	bool run_out_mapped_page;

	uint64_t page_size;
    uint64_t page_count;
	uint64_t page_use;
	

	// char *shared_mem;

	char *shared_mem_start;

	char *shared_mem_start_end;

	
	PFMonitor::Callback notify_page_fault(PFMonitor::addr_t addr);
	PFMonitor::addr_t get_avaible_page();
	
	PFMonitor::addr_t evict_a_page(PFMonitor::addr_t start, PFMonitor::addr_t end);
	int map_new_page_from_blkdev(PFMonitor::addr_t pf_addr, PFMonitor::addr_t map_addr);

	
public:
  Controller() {}
  /** @brief Initialize the internal state **/
  int init();

};