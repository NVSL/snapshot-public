#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh
ROOT="${DIR}/../../"

LOG_F="$(mktemp)"

LL_ROOT="${ROOT}/src/examples/"
LL_CXLBUF="linkedlist_tx/linkedlist_tx"
LL_PMDK="linkedlist_pmdk/linkedlist_pmdk"

if [ -z "${1-}" ]; then
    printf "USAGE:\n\t${BASH_SOURCE[0]} (GPU|MSS|PMEM)\n" >&2
    exit 1
fi

FS_POOL="/mnt/pmem0p2/linkedlist"
FS_DJ_POOL="/mnt/pmem0p2/map"

OPS=$MILLION
EXP="$1"

if [ "$1" = "MSS" ]; then
    PMDK_POOL="/mnt/mss0/linkedlist"
    LD_PRELOAD="${ROOT}/src/examples/redirect/libredirect.so"
    CXLBUF_LOG_LOC="/mnt/mss0/cxlbuf_logs"
    
    LL_CXLBUF="linkedlist_tx_cxlfs/linkedlist_tx_cxlfs"
    OPS=$((5*$MILLION))
elif [ "$1" = "GPU" ]; then
    PMDK_POOL="/mnt/cxl0/linkedlist"
    LD_PRELOAD="${ROOT}/src/examples/redirect/libredirect.so"
    CXLBUF_LOG_LOC="/mnt/cxl0/cxlbuf_logs"
else
    PMDK_POOL="/mnt/pmem0/linkedlist"
    LD_PRELOAD=""
    CXLBUF_LOG_LOC="/mnt/pmem0/cxlbuf_logs"
fi

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0
export CXLBUF_USE_HUGEPAGE=1
export PMEM_IS_PMEM_FORCE=1
set -e
# set -x

check_bin "${LL_ROOT}/${LL_PMDK}" "-O3"
check_bin "${LL_ROOT}/${LL_CXLBUF}" "-O3"
check_bin "${LL_ROOT}/${LL_CXLBUF}" "-D RELEASE"

clean() {
    rm -f "${PMDK_POOL}"*
    rm -f "${FS_POOL}"*
    rm -f "${FS_DJ_POOL}"*
    rm -rf "${CXLBUF_LOG_LOC}"
    rm -rf /mnt/mss0/*
}

execute() {
    OP="$1"    # Operation to perform
    OCCUR="$2" # Only use the n'th occurrence
    NAME="$3"  # Name of the workload

    printf "${NAME},"
    clean
    pmdk=$(printf "$OP" | LD_PRELOAD="${LD_PRELOAD}" "${LL_ROOT}/${LL_PMDK}" "${PMDK_POOL}"  2>&1 \
        | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n')

    printf "${pmdk},"

    clean
    cxlbuf=$(printf "$OP" | CXLBUF_LOG_LOC="${CXLBUF_LOG_LOC}" CXL_MODE_ENABLED=1 "${LL_ROOT}/${LL_CXLBUF}" "${PMDK_POOL}" 2>&1 \
                 | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n')

    printf "${cxlbuf}"

    if [ "$EXP" != "MSS" ]; then
        printf ","
        clean
        printf "$OP" | CXLBUF_USE_HUGEPAGE=0 "${LL_ROOT}/${LL_CXLBUF}" "${FS_POOL}" 2>&1 \
            | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n'

        printf ","

        clean
        printf "$OP" | "${LL_ROOT}/${LL_CXLBUF}" "${FS_POOL}" 2>&1 \
            | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n'

        printf ","

        clean
        printf "$OP" | CXLBUF_USE_HUGEPAGE=0 "${LL_ROOT}/${LL_CXLBUF}" "${FS_DJ_POOL}" 2>&1 \
            | grep 'Total ns' | head -n"$OCCUR" | tail -n1 | grep -Eo '[0-9]+' | tr -d '\n'
    fi
    printf "\n"
    
}

printf "workload,pmdk,snashot"
if [ "$1" != "MSS" ]; then
    printf ",msync,msync huge page,msync data journal"
fi
printf "\n"

execute "A $OPS" 1 "insert"
execute "A $OPS\nR $OPS" 2 "delete"
execute 'A '$OPS'\ns' 2 "traverse"


