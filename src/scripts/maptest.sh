#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh


MAP_ROOT="/home/smahar/git/cxlbuf/src/examples/"
MAP_PMDK="map_pmdk/mapcli"
MAP_CXLBUF="map/map"
PMDK_POOL="/mnt/pmem0/map"
FS_DJ_POOL="/mnt/pmem0p2/map"
FS_POOL="/mnt/pmem0p3/map"
SCALE=4

OPS=400000

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0
export CXLBUF_USE_HUGEPAGE=1
set -e
# set -x

clear() {
    rm -f "${PMDK_POOL}"*
    rm -f "${FS_POOL}"*
    rm -f "${FS_DJ_POOL}"*
    rm -rf "${LOG_LOC}"    
}

using_perf() {
    name=$1
    sudo -E perf stat -o "/tmp/perf.$name" -d -d -d -e 'L1-dcache-load-misses,L1-dcache-loads,L1-dcache-stores,L1-icache-load-misses,LLC-load-misses,LLC-loads,LLC-store-misses,LLC-stores' "${@:2}"
}

LOG_F=/tmp/output.log

execute_insert() {
    printf "insert,"

    # PMDK
    clear
    echo 'k\n' | "${MAP_ROOT}/${MAP_PMDK}" btree "${PMDK_POOL}"  2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Elapsed s = ' \
        | sed 's/Elapsed s = //g' \
        | tr -d '\n'

    # Snapshot
    clear
    val=$(CXL_MODE_ENABLED=1 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk i $OPS 2>&1 \
        | tee "$LOG_F" \
        | grep 'Total ns' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf ",$(bc <<< "scale=$SCALE; $val/1000000000")"

    # msync - no huge pages
    clear
    val=$(CXLBUF_USE_HUGEPAGE=0 CXL_MODE_ENABLED=0 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk i $OPS 2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Total ns' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf ",$(bc <<< "scale=$SCALE; $val/1000000000")"

    # msync - huge pages
    clear
    val=$(CXLBUF_USE_HUGEPAGE=1 CXL_MODE_ENABLED=0 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk i $OPS 2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Total ns' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf ",$(bc <<< "scale=$SCALE; $val/1000000000")"

    # msync - data journal
    clear
    val=$(CXLBUF_USE_HUGEPAGE=1 CXL_MODE_ENABLED=0 "${MAP_ROOT}/${MAP_CXLBUF}" "${FS_DJ_POOL}" btree bulk i $OPS 2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Total ns' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf ",$(bc <<< "scale=$SCALE; $val/1000000000")"
}

execute_delete() {
        printf "\ndelete,"

    # PMDK
    clear
    echo 'R\n' | "${MAP_ROOT}/${MAP_PMDK}" btree "${PMDK_POOL}"  2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Elapsed s = ' \
        | sed 's/Elapsed s = //g' \
        | tr -d '\n'

    printf ","

    # Snapshot
    clear
    val=$(CXL_MODE_ENABLED=1 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk r $OPS 2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Total ns' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf "$(bc <<< "scale=$SCALE; $val/1000000000"),"

    # msync - no huge page
    clear
    val=$(CXLBUF_USE_HUGEPAGE=0 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk r $OPS 2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Total ns: ' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf "$(bc <<< "scale=$SCALE; $val/1000000000"),"

    # msync - huge page
    clear
    val=$(CXLBUF_USE_HUGEPAGE=1 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk r $OPS 2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Total ns: ' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf "$(bc <<< "scale=$SCALE; $val/1000000000"),"

    # msync - data journal
    clear
    val=$(CXLBUF_USE_HUGEPAGE=1 "${MAP_ROOT}/${MAP_CXLBUF}" "${FS_POOL}" btree bulk r $OPS 2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Total ns: ' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf "$(bc <<< "scale=$SCALE; $val/1000000000")\n"
}

execute_traverse() {
    printf "read,"

    # PMDK
    clear
    echo 'C\n' | "${MAP_ROOT}/${MAP_PMDK}" btree "${PMDK_POOL}"  2>&1 \
        | grep 'Elapsed s = ' \
        | sed 's/Elapsed s = //g' \
        | tr -d '\n'

    printf ","

    # Snapshot
    clear
    val=$(CXLBUF_USE_HUGEPAGE=1 CXL_MODE_ENABLED=1 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk c $OPS 2>&1 \
              | grep 'Total ns' \
              | sed 's/Total ns: //g'\
              | tr -d '\n')

    printf "$(bc <<< "scale=$SCALE; $val/1000000000"),"

    # msync - no huge page
    clear
    val=$(CXLBUF_USE_HUGEPAGE=0 CXL_MODE_ENABLED=0 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk c $OPS 2>&1 \
              | grep 'Total ns' \
              | sed 's/Total ns: //g'\
              | tr -d '\n')

    printf "$(bc <<< "scale=$SCALE; $val/1000000000"),"

    # msync - use huge page
    clear
    val=$(CXLBUF_USE_HUGEPAGE=1 CXL_MODE_ENABLED=0 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk c $OPS 2>&1 \
              | grep 'Total ns' \
              | sed 's/Total ns: //g'\
              | tr -d '\n')

    printf "$(bc <<< "scale=$SCALE; $val/1000000000"),"

    # msync data journal
    clear
    val=$(CXLBUF_USE_HUGEPAGE=0 CXL_MODE_ENABLED=0 "${MAP_ROOT}/${MAP_CXLBUF}" "${FS_POOL}" btree bulk c $OPS 2>&1 \
              | grep 'Total ns' \
              | sed 's/Total ns: //g'\
              | tr -d '\n')

    printf "$(bc <<< "scale=$SCALE; $val/1000000000")\n"
}

execute() {
    ####################
    ######## INSERT
    ####################

    execute_insert

    ####################
    ######## DELETE
    ####################

    execute_delete

    ####################
    ######## TRAVERSE
    ####################

    execute_traverse
}

printf "operation,pmdk,snapshot,msync,msync huge pages,msync data journal\n"
execute



