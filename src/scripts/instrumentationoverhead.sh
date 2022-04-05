#!/usr/bin/env bash

SIMPLEKV_ROOT="/home/smahar/git/cxlbuf/src/examples/"
SIMPLEKV_CXLBUF="simplekv_cxlbuf/simplekv_puddles"
SIMPLEKV_NOSYNC="simplekv_cxlbuf/simplekv_cxlbuf"
YCSB_LOC="/home/smahar/git/libpuddles-scripts/traces/"
PMDK_POOL="/mnt/pmem0/simplekv"

YCSB_WRKLD="c"

OPS=100000

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0

set -e
# set -x

execute() {

    rm -f "${PMDK_POOL}"*
    rm -f "${LOG_LOC}"

    CXL_MODE_ENABLED=1 "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.inst" "${PMDK_POOL}" \
                       ycsb "${YCSB_LOC}${wrkld}-load-1.0" \
                       "${YCSB_LOC}${wrkld}-run-1.0"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf ","

    rm -f "${PMDK_POOL}"*
    rm -f "${LOG_LOC}"

    "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.no_sync" "${PMDK_POOL}" ycsb \
                                        "${YCSB_LOC}${wrkld}-load-1.0" \
                                        "${YCSB_LOC}${wrkld}-run-1.0"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf "\n"
}

printf "instrumented,uninstrumented\n"
for wrkld in $YCSB_WRKLD; do
    execute
done


