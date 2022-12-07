#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh
ROOT="${DIR}/../../"

MAP_ROOT="${ROOT}/src/examples/"
MAP_PMDK="map_pmdk/mapcli"
MAP_CXLBUF="map/map"
PMDK_POOL="/mnt/pmem0/map"
FS_DJ_POOL="/mnt/pmem0p2/map"
FS_POOL="/mnt/pmem0p3/map"
SCALE=4
LD_PRELOAD=""

OPS=10000000
EXP="$1"

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export PMEM_IS_PMEM_FORCE=1
export CXL_MODE_ENABLED=0
export CXLBUF_USE_HUGEPAGE=1
set -e
# set -x

if [ -z "${1-}" ]; then
    printf "USAGE:\n\t${BASH_SOURCE[0]} (MSS|PMEM)\n" >&2
    exit 1
fi

if [ "$EXP" = "MSS" ]; then
    PMDK_POOL="/mnt/mss0/map"
    LD_PRELOAD="${ROOT}/src/examples/redirect/libredirect.so"
    export CXLBUF_LOG_LOC="/mnt/mss0/cxlbuf_logs/"
fi


clear() {
    rm -f "${PMDK_POOL}"*
    rm -f "${FS_POOL}"*
    rm -f "${FS_DJ_POOL}"*
    rm -rf "${LOG_LOC}"
    rm -rf /mnt/mss0/*
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
    echo 'k\n' | LD_PRELOAD="${LD_PRELOAD}" "${MAP_ROOT}/${MAP_PMDK}" btree "${PMDK_POOL}"  2>&1 \
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

    if [ "$EXP" = "MSS" ]; then
        return
    fi

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
    echo 'R\n' | LD_PRELOAD="${LD_PRELOAD}" "${MAP_ROOT}/${MAP_PMDK}" btree "${PMDK_POOL}"  2>&1 \
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

    printf "$(bc <<< "scale=$SCALE; $val/1000000000")"

    if [ "$EXP" = "MSS" ]; then
        printf "\n"
        return
    fi

    # msync - no huge page
    clear
    val=$(CXLBUF_USE_HUGEPAGE=0 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk r $OPS 2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Total ns: ' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf ",$(bc <<< "scale=$SCALE; $val/1000000000"),"

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
    echo 'C\n' | LD_PRELOAD="${LD_PRELOAD}" "${MAP_ROOT}/${MAP_PMDK}" btree "${PMDK_POOL}"  2>&1 \
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

    printf "$(bc <<< "scale=$SCALE; $val/1000000000")"

    if [ "$EXP" = "MSS" ]; then
        return
    fi

    # msync - no huge page
    clear
    val=$(CXLBUF_USE_HUGEPAGE=0 CXL_MODE_ENABLED=0 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk c $OPS 2>&1 \
              | grep 'Total ns' \
              | sed 's/Total ns: //g'\
              | tr -d '\n')

    printf ",$(bc <<< "scale=$SCALE; $val/1000000000"),"

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

printf "operation,pmdk,snapshot"
if [ "$EXP" != "MSS" ]; then
    printf ",msync,msync huge pages,msync data journal"
fi
printf "\n"

execute

if [ "$EXP" = "MSS" ]; then
    printf "\n"
fi


