#!/usr/bin/env bash

SIMPLEKV_ROOT="/home/smahar/git/cxlbuf/src/examples/"
SIMPLEKV_CXLBUF="simplekv_cxlbuf/simplekv_puddles"
SIMPLEKV_PMDK="simplekv_pmdk/simplekv_pmdk"
YCSB_LOC="/home/smahar/git/libpuddles-scripts/traces/"
PMDK_POOL="/mnt/pmem0/simplekv"

YCSB_WRKLD="a b c d e f g"

OPS=100000

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0

set -e
# set -x

execute() {
    rm -f "${PMDK_POOL}"

    pmempool create obj --layout=simplekv -s 1G "${PMDK_POOL}"
    
    "${SIMPLEKV_ROOT}/${SIMPLEKV_PMDK}" "${PMDK_POOL}" ycsb \
                                        "${YCSB_LOC}${wrkld}-load-1.0" \
                                        "${YCSB_LOC}${wrkld}-run-1.0"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf ","

    rm -f "${PMDK_POOL}"
    fallocate --length 1G /mnt/pmem0/simplekv
    CXL_MODE_ENABLED=1 "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.inst" "${PMDK_POOL}" \
                       ycsb "${YCSB_LOC}${wrkld}-load-1.0" \
                       "${YCSB_LOC}${wrkld}-run-1.0"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf ","

    rm -f "${PMDK_POOL}"
    fallocate --length 1G /mnt/pmem0/simplekv
    "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}" "${PMDK_POOL}" ycsb \
                                        "${YCSB_LOC}${wrkld}-load-1.0" \
                                        "${YCSB_LOC}${wrkld}-run-1.0"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf "\n"
}

printf "pmdk,snashot,msync\n"
for wrkld in $YCSB_WRKLD; do
    execute
done


