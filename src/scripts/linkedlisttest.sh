#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh
ROOT="${DIR}/../../"

LL_ROOT="${ROOT}/src/examples/"
LL_CXLBUF="linkedlist_tx/linkedlist_tx"
LL_PMDK="linkedlist_pmdk/linkedlist_pmdk"

if [ -z "${1-}" ]; then
    printf "USAGE:\n\t${BASH_SOURCE[0]} (GPU|PMEM)\n" >&2
    exit 1
fi

if [ "$1" = "GPU" ]; then
    PMDK_POOL="/mnt/cxl0/linkedlist"
    LD_PRELOAD="${ROOT}/src/examples/redirect/libredirect.so"
    CXLBUF_LOG_LOC="/mnt/cxl0/cxlbuf_logs"
else
    PMDK_POOL="/mnt/pmem0/linkedlist"
    LD_PRELOAD=""
    CXLBUF_LOG_LOC="/mnt/pmem0/cxlbuf_logs"
fi

OPS=$HUNDRED_K

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0

set -e
# set -x

check_bin "${LL_ROOT}/${LL_PMDK}" "-O3"
check_bin "${LL_ROOT}/${LL_CXLBUF}" "-O3"
check_bin "${LL_ROOT}/${LL_CXLBUF}" "-D RELEASE"

execute() {
    OP="$1"    # Operation to perform
    OCCUR="$2" # Only use the n'th occurrence
    NAME="$3"  # Name of the workload

    printf "${NAME},"

    rm -f "${PMDK_POOL}"*
    rm -rf "${CXLBUF_LOG_LOC}"

    #pmempool create obj --layout=linkedlist -s 256M "${PMDK_POOL}"
    
    pmdk=$(printf "$OP" | LD_PRELOAD="${LD_PRELOAD}" "${LL_ROOT}/${LL_PMDK}" "${PMDK_POOL}"  2>&1 \
        | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n')

    printf "${pmdk},"

    rm -f "${PMDK_POOL}"*
    rm -rf "${CXLBUF_LOG_LOC}"
    cxlbuf=$(printf "$OP" | CXLBUF_LOG_LOC="${CXLBUF_LOG_LOC}" CXL_MODE_ENABLED=1 "${LL_ROOT}/${LL_CXLBUF}" "${PMDK_POOL}" 2>&1 \
                 | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n')

    printf "${cxlbuf},"

    # return

    rm -f "${PMDK_POOL}*"
    rm -rf "${CXLBUF_LOG_LOC}"
    printf "$OP" | "${LL_ROOT}/${LL_CXLBUF}" "${PMDK_POOL}" 2>&1 \
        | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n'

    printf "\n"
}

printf "workload,pmdk,snashot,msync\n"
execute "A $OPS" 1 "insert"
execute "A $OPS\nR $OPS" 2 "delete"
execute 'A '$OPS'\ns' 2 "traverse"


