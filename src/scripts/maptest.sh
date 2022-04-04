#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh


MAP_ROOT="/home/smahar/git/cxlbuf/src/examples/"
MAP_PMDK="map_pmdk/mapcli"
MAP_CXLBUF="map/map"
PMDK_POOL="/mnt/pmem0/map"
SCALE=4

OPS=100000

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0

set -e
# set -x

execute() {
    printf "insert,"
    
    rm -f "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"

    echo 'k\n' | "${MAP_ROOT}/${MAP_PMDK}" btree "${PMDK_POOL}"  2>&1 \
        | grep 'Elapsed s = ' \
        | sed 's/Elapsed s = //g' \
        | tr -d '\n'

    rm -f "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"

    val=$(CXL_MODE_ENABLED=1 "${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk i 400000 2>&1 \
        | grep 'Total ns' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf ",$(bc <<< "scale=$SCALE; $val/1000000000")\ndelete,"


    rm -f "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"

    echo 'R\n' | "${MAP_ROOT}/${MAP_PMDK}" btree "${PMDK_POOL}"  2>&1 \
        | grep 'Elapsed s = ' \
        | sed 's/Elapsed s = //g' \
        | tr -d '\n'

    printf ","

    rm -f "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"

    val=$("${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk r 400000 2>&1 \
        | grep 'Total ns: ' \
        | sed 's/Total ns: //g' \
        | tr -d '\n')

    printf "$(bc <<< "scale=$SCALE; $val/1000000000")\nread,"


    rm -f "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"

    echo 'C\n' | "${MAP_ROOT}/${MAP_PMDK}" btree "${PMDK_POOL}"  2>&1 \
        | grep 'Elapsed s = ' \
        | sed 's/Elapsed s = //g' \
        | tr -d '\n'

    printf ","

    rm -f "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"

    val=$("${MAP_ROOT}/${MAP_CXLBUF}" "${PMDK_POOL}" btree bulk c 400000 2>&1 \
              | grep 'Total ns' \
              | sed 's/Total ns: //g'\
              | tr -d '\n')
    


    printf "$(bc <<< "scale=$SCALE; $val/1000000000")\n"
}

printf "operation,pmdk,snapshot\n"
execute



