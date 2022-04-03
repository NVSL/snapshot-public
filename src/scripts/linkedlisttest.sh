#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh

LL_ROOT="/home/smahar/git/cxlbuf/src/examples/"
LL_CXLBUF="linkedlist_tx/linkedlist_tx"
LL_PMDK="linkedlist_pmdk/linkedlist_pmdk"
PMDK_POOL="/mnt/pmem0/linkedlist"

OPS=$HUNDRED_K

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0

set -e
# set -x

execute() {
    OP="$1"    # Operation to perform
    OCCUR="$2" # Only use the n'th occurrence
    NAME="$3"  # Name of the workload

    printf "${NAME},"

    rm -f "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"

    pmempool create obj --layout=linkedlist -s 256M "${PMDK_POOL}"
    
    pmdk=$(printf "$OP" | "${LL_ROOT}/${LL_PMDK}" "${PMDK_POOL}"  2>&1 \
        | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n')

    printf "${pmdk},"

    rm -f "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"
    cxlbuf=$(printf "$OP" | CXL_MODE_ENABLED=1 "${LL_ROOT}/${LL_CXLBUF}" "${PMDK_POOL}" 2>&1 \
                 | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n')

    printf "${cxlbuf},"

    rm -f "${PMDK_POOL}*"
    rm -rf "${LOG_LOC}"
    printf "$OP" | "${LL_ROOT}/${LL_CXLBUF}" "${PMDK_POOL}" 2>&1 \
        | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n'

    printf "\n"
}

printf "workload,pmdk,snashot,msync\n"
execute "A $OPS" 1 "insert"
execute "A $OPS\nR $OPS" 2 "delete"
execute 'A '$OPS'\ns' 2 "traverse"


