#!/usr/bin/env bash

SIMPLEKV_ROOT="/home/smahar/git/cxlbuf/src/examples/"
SIMPLEKV_CXLBUF="simplekv_cxlbuf/simplekv_puddles"
SIMPLEKV_PMDK="simplekv_pmdk/simplekv_pmdk"
SIMPLEKV_NOSYNC="simplekv_cxlbuf/simplekv_cxlbuf"
YCSB_LOC="/home/smahar/git/libpuddles-scripts/traces/"
PMDK_POOL="/mnt/pmem0/simplekv"

YCSB_WRKLD="a b c d e f g"

OPS=100000

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0

set -e
# set -x

delete_files() {
    rm -f "${PMDK_POOL}"*
    rm -f "${LOG_LOC}"
}

execute() {
    BIN="$1"

    "${BIN}" "${PMDK_POOL}" ycsb "${YCSB_LOC}${wrkld}-load-1.0" \
             "${YCSB_LOC}${wrkld}-run-1.0" 2>&1
}

get_msync_time() {
    result=$(CXL_MODE_ENABLED=1 execute "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.inst")

    echo "$result" | grep 'perst_overhead = ' | sed 's|perst_overhead = ||'\
        | tr -d ' '| tail -n1
}

run() {
    delete_files

    msync_time=$(get_msync_time)

    printf "${msync_time},"

    delete_files

    CXL_MODE_ENABLED=1 CXLBUF_MSYNC_IS_NOP=1 CXLBUF_MSYNC_SLEEP_NS=${msync_time} \
                       execute "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}" \
        | grep -m1 'ns = ' | head -n1 | grep -Eo '[0-9]+' | tr -d '\n' 
    printf ","

    delete_files

    CXLBUF_MSYNC_IS_NOP=1 CXLBUF_MSYNC_SLEEP_NS=${msync_time} CXL_MODE_ENABLED=1 \
                          execute "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.inst" \
        | grep -m1 'ns = ' | head -n1 | grep -Eo '[0-9]+' | tr -d '\n'

    printf "\n"
}

printf "workload,msynctime,uninstrumented,instrumented\n"
for wrkld in $YCSB_WRKLD; do
    printf "${wrkld},"
    run
done


