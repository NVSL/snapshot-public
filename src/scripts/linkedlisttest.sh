#!/usr/bin/env bash

LL_ROOT="/home/smahar/git/cxlbuf/src/examples/"
LL_CXLBUF="linkedlist_tx/linkedlist_tx"
LL_PMDK="linkedlist_pmdk/linkedlist_pmdk"
PMDK_POOL="/mnt/pmem0/linkedlist"

OPS=100000

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0

set -e
# set -x

execute() {
    OP="$1"

    rm -f "${PMDK_POOL}"

    # pmempool create obj --layout=linkedlist -s 256M "${PMDK_POOL}"
    
    pmdk=$(echo "$OP" | "${LL_ROOT}/${LL_PMDK}" "${PMDK_POOL}"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n')

    printf "${pmdk},"
    rm -f "${PMDK_POOL}"
    cxlbuf=$(echo "$OP" | CXL_MODE_ENABLED=1 "${LL_ROOT}/${LL_CXLBUF}" "${PMDK_POOL}" 2>&1 \
                 | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n')
    

    printf "${cxlbuf},"

    printf "\npmdk/cxlbuf:$(bc <<< "scale=2; $pmdk/$cxlbuf")\n"

    rm -f "${PMDK_POOL}"
    echo "$OP" | "${LL_ROOT}/${LL_CXLBUF}" "${PMDK_POOL}" 2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf "\n"
}

printf "pmdk,snashot,msync\n"
execute "A $OPS"


