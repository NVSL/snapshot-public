// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   pfcontroller.cc
 * @date   novembre  16, 2022
 * @brief  Controller to deal with page fault
 */

#pragma once
#include <functional>

#include "libcxlfs/controller.hh"

#include "nvsl/common.hh"
#include "nvsl/stats.hh"
#include "nvsl/utils.hh"

using nvsl::RCast;

addr_t* Controller::get_avaible_page() {
    auto page_addr = RCast<uint64_t *>(((shared_mem_start >> 12) + page_use) << 12);
    
    page_use +=1;

    if(page_use == page_size) {
        run_out_mapped_page = true;
    }

    return page_addr;
}

addr_t* Controller::evict_a_page(addr_t *start_addr, addr_t *end_addr) {
    auto start = RCast<uint64_t>(start);
    auto end = RCast<uint64_t>(end);
    
    auto dist = mbd->get_dist(start, end);
    
    addr_t target_page;

    for(uint64_t  i = 0; i < sizeof(dist)/sizeof(heat_t); i++){
        if(dist[i]){
            target_page = i;
            break;
        }
    }

    auto addr = target_page << 12;

    return RCast<uint64_t *>addr;
}


void Controller::notify_page_fault(addr_t addr) {

    if(!run_out_mapped_page) {
        page_addr = evict_a_page(shared_mem_start, shared_mem_start_end);
        map_new_page_from_blkdev((void *)addr, page_addr);
    } else {
        page_addr = get_avaible_page();
        map_new_page_from_blkdev((void *)addr, page_addr);
    }
}


int Controller::map_new_page_from_blkdev(addr_t *pf_addr, addr_t *map_addr) {

    char *buf = malloc(page_size);

    auto addr_page = (pf_addr >> 12) << 12;
    auto start_lba = (pf_addr >> 12) * 8;

    ubd->read_blocking(buf, start_lba, 8);

    memcpy(map_addr, buf, page_size);
    return 1;


}


/** @brief Initialize the internal state **/
int Controller::init(){
    ubd = new UserBlkDev;
    pfm = new pfmonitor;
    mbd = new membwdist;

    ubd->init();
    pfm->init();

    run_out_mapped_page = false;
    
    page_size = 0x1000;
    page_count = 1000;
    page_use = 0;

    shared_mem_start = malloc(page_size * page_count);
    shared_mem_start_end = page_size * page_count + shared_mem_start;

    pfm->register_range(shared_mem_start, shared_mem_start_end);

    pfm->monitor(notify_page_fault);


}