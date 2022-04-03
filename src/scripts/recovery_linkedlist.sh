#!/usr/bin/env sh

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=1

rm -rf /mnt/pmem0/cxlbuf_logs /mnt/pmem0/linkedlist*

printf "============================================================\n"
echo "i 1" | /home/smahar/git/cxlbuf/src/examples/linkedlist_tx/linkedlist_tx /mnt/pmem0/linkedlist

printf "============================================================\n"
echo "i 2" | CXLBUF_CRASH_ON_COMMIT=1  /home/smahar/git/cxlbuf/src/examples/linkedlist_tx/linkedlist_tx /mnt/pmem0/linkedlist

printf "============================================================\n"
echo "p" | CXLBUF_CRASH_ON_COMMIT=1  /home/smahar/git/cxlbuf/src/examples/linkedlist_tx/linkedlist_tx /mnt/pmem0/linkedlist
