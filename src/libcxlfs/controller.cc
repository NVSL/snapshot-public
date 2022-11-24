// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   pfcontroller.cc
 * @date   novembre  16, 2022
 * @brief  Controller to deal with page fault
 */


#include <functional>

#include "libcxlfs/controller.hh"

#include "nvsl/common.hh"
#include "nvsl/stats.hh"
#include "nvsl/utils.hh"
#include "nvsl/error.hh"

using nvsl::RCast;

PFMonitor::addr_t Controller::get_avaible_page() {
    const auto shared_mem_start_npt = RCast<uint64_t>(RCast<char *>(shared_mem_start));
    const auto page_addr = ((shared_mem_start_npt >> 12) + page_use) << 12;
    
    page_use +=1;

    if(page_use == page_size) {
        run_out_mapped_page = true;
    }

    return page_addr;
}

PFMonitor::addr_t Controller::evict_a_page(PFMonitor::addr_t start_addr, PFMonitor::addr_t end_addr) {
    // const auto start = RCast<uint64_t>(start);
    // const auto end = RCast<uint64_t>(end);
    
    const auto dist = mbd->get_dist(start_addr, end_addr);
    
    PFMonitor::addr_t target_page;

    for(uint64_t  i = 0; i < page_count; i++){
        if(dist[i]){
            target_page = i;
            break;
        }
    }

    const auto addr = target_page << 12;

    return addr;
}

// PFMonitor::Callback Controller::notify_page_fault(PFMonitor::addr_t addr) {
//     if(run_out_mapped_page) {
//         const auto page_addr = evict_a_page(RCast<uint64_t>(RCast<char *>(shared_mem_start)), RCast<uint64_t>(RCast<char *>(shared_mem_start)_end));
//         map_new_page_from_blkdev(addr, page_addr);
//     } else {
//         const auto page_addr = get_avaible_page();
//         map_new_page_from_blkdev(addr, page_addr);
//     }

//     return nullptr;

// }



int Controller::map_new_page_from_blkdev(PFMonitor::addr_t pf_addr, PFMonitor::addr_t map_addr) {

    char *buf = RCast<char *>(malloc(page_size));

  
    const auto start_lba = (pf_addr >> 12) * 8;


    ubd->read_blocking(buf, start_lba, 8);

    const auto map_addr_pt = RCast<uint64_t*>(map_addr);
    memcpy(map_addr_pt, buf, page_size);
    return 1;


}




/** @brief Initialize the internal state **/
int Controller::init(){
    ubd = new UserBlkDev();
    pfm = new PFMonitor();
    mbd = new MemBWDist();

    ubd->init();
    pfm->init();

    run_out_mapped_page = false;
    
    page_size = 0x1000;
    page_count = 1000;
    page_use = 0;

    // RCast<char *>(shared_mem_start) = RCast<char*>(malloc(page_size * page_count));

    const auto temp = RCast<char *>(mmap(nullptr, page_size*page_count, PROT_READ | PROT_WRITE,
         MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));

    
    shared_mem_start = RCast<char *>(temp);
    
    // RCast<char *>(shared_mem_start)_end = page_size * page_count + RCast<char *>(shared_mem_start);
    
    



    pfm->register_range(RCast<uint64_t*>(RCast<char *>(shared_mem_start)), page_size * page_count);
   
    PFMonitor::Callback notify_page_fault = [&](PFMonitor::addr_t addr) {
        if(run_out_mapped_page) {
            const auto page_addr = evict_a_page(RCast<uint64_t>(RCast<char *>(shared_mem_start)), RCast<uint64_t>(shared_mem_start_end));
            map_new_page_from_blkdev(addr, page_addr);
        } else {
            const auto page_addr = get_avaible_page();
            map_new_page_from_blkdev(addr, page_addr);
        }

  };
    
   
    

    pfm->monitor(notify_page_fault);

    return 1;

}

char* Controller::getSharedMemAddr() {
    return RCast<char *>(shared_mem_start);
}

uint64_t Controller::getSharedMemSize() {
    return page_size;
}