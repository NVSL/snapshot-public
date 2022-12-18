#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh
ROOT="${DIR}/../../"

SIMPLEKV_ROOT="${ROOT}/src/examples/"
CONFIG_FILE="${SIMPLEKV_ROOT}../../make.config"
SIMPLEKV_CXLBUF="simplekv_cxlbuf/simplekv_puddles"
SIMPLEKV_PMDK="simplekv_pmdk/simplekv_pmdk"
YCSB_LOC="${ROOT}/data/traces/"
PMDK_POOL="/mnt/pmem0/simplekv"
FS_DJ_POOL="/mnt/pmem0p2/simplekv"
FS_POOL="/mnt/pmem0p3/simplekv"
V="${V:-0}"
LOG_F=`mktemp`

NUMA_CTL="numactl -N0 --"
NUMA_CTL_SNAPSHOT=""
LD_PRELOAD=""
EXP="$1"

YCSB_WRKLD="a b c d e f g"
# YCSB_WRKLD="a c g"

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0
export CXLBUF_USE_HUGEPAGE=1

set -e
set -o pipefail
#set -x

if [ -z "${1-}" ]; then
    printf "USAGE:\n\t${BASH_SOURCE[0]} (MSS|PMEM)\n" >&2
    exit 1
fi

if [ "$EXP" = "MSS" ]; then
    PMDK_POOL="/mnt/mss0/simplekv"
    LD_PRELOAD="${ROOT}/src/examples/redirect/libredirect.so"
    export CXLBUF_LOG_LOC="/mnt/mss0/cxlbuf_logs/"
    export REMOTE_NODE=0
else
#    export CXLBUF_LOG_LOC="/mnt/pmem0/cxlbuf_logs/"
    NUMA_CTL_SNAPSHOT="numactl -N0 --"
fi


recompile() {
    make clean -C "$(dirname "$CONFIG_FILE")"
    make release -j$(nproc) -C "$(dirname "$CONFIG_FILE")"
}

remake() {
    if [ -z "$LLVM_DIR" ]; then
        echo "LLVM_DIR not set"
        exit 1
    fi

   
    if [ "$1" = "nonvolatile" ]; then
        sed -i 's|LOG_FORMAT_VOLATILE=y|LOG_FORMAT_VOLATILE=n|g' "$CONFIG_FILE"
        sed -i 's|LOG_FORMAT_NON_VOLATILE=n|LOG_FORMAT_NON_VOLATILE=y|g' "$CONFIG_FILE"
    else
        sed -i 's|LOG_FORMAT_VOLATILE=n|LOG_FORMAT_VOLATILE=y|g' "$CONFIG_FILE"
        sed -i 's|LOG_FORMAT_NON_VOLATILE=y|LOG_FORMAT_NON_VOLATILE=n|g' "$CONFIG_FILE"
    fi

    if [ "$2" = "align" ]; then
        sed -i 's|CXLBUF_ALIGN_SNAPSHOT_WRITES=n|CXLBUF_ALIGN_SNAPSHOT_WRITES=y|g' "$CONFIG_FILE"
    else
        sed -i 's|CXLBUF_ALIGN_SNAPSHOT_WRITES=y|CXLBUF_ALIGN_SNAPSHOT_WRITES=n|g' "$CONFIG_FILE"
    fi

    if [ "$V" = "1" ]; then 
        recompile
    else
        echo "Recompiling $(dirname "$CONFIG_FILE")" >&2
        recompile >>"$LOG_F" 2>&1
    fi
}

clean() {
    rm -rf "${PMDK_POOL}"*
    rm -rf "${FS_POOL}"*
    rm -rf "${FS_DJ_POOL}"*
    rm -rf "${LOG_LOC}"
}

execute() {
    clean

    LD_PRELOAD="${LD_PRELOAD}" ${NUMA_CTL}  "${SIMPLEKV_ROOT}/${SIMPLEKV_PMDK}" "${PMDK_POOL}" ycsb \
                                        "${YCSB_LOC}${wrkld}.load" \
                                        "${YCSB_LOC}${wrkld}.run"  2>&1 \
        | tee -a "$LOG_F" \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf ","

    clean

    CXL_MODE_ENABLED=1 "${NUMA_CTL_SNAPSHOT}" "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.inst" "${PMDK_POOL}" \
                       ycsb "${YCSB_LOC}${wrkld}.load" \
                       "${YCSB_LOC}${wrkld}.run"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    if [ "$EXP" = "MSS" ]; then
        printf "\n"
        return
    fi
    
    printf ","

    clean

    CXLBUF_USE_HUGEPAGE=0 CXL_MODE_ENABLED=0 ${NUMA_CTL} "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.inst" "${FS_POOL}" \
                       ycsb "${YCSB_LOC}${wrkld}.load" \
                       "${YCSB_LOC}${wrkld}.run"  2>&1 | tee -a "${LOG_F}"\
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf ","

    clean

    CXLBUF_USE_HUGEPAGE=1 CXL_MODE_ENABLED=0 ${NUMA_CTL} "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.inst" "${FS_POOL}" \
                       ycsb "${YCSB_LOC}${wrkld}.load" \
                       "${YCSB_LOC}${wrkld}.run"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf ","
    clean

    CXLBUF_USE_HUGEPAGE=0 CXL_MODE_ENABLED=0 ${NUMA_CTL} "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}" "${FS_DJ_POOL}" \
                       ycsb "${YCSB_LOC}${wrkld}.load" \
                       "${YCSB_LOC}${wrkld}.run"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    clean

    printf "\n"
}

execute_snapshot() {
    rm -rf "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"

    CXLBUF_USE_HUGEPAGE=1 CXL_MODE_ENABLED=1 "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.inst" "${PMDK_POOL}" \
                       ycsb "${YCSB_LOC}${wrkld}.load" \
                       "${YCSB_LOC}${wrkld}.run"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'


    printf "\n"
}


remake volatile align
printf "pmdk,snashot"
if [ "$EXP" != "MSS" ]; then
    printf ",msync,msync huge page,msync data journal"
fi
printf "\n"

for wrkld in $YCSB_WRKLD; do
    execute;
done

if [ "$EXP" = "MSS" ]; then
    exit
fi

remake nonvolatile align
printf "pmdk,snashot,msync,msync huge page,msync data journal\n"
for wrkld in $YCSB_WRKLD; do
    execute
done

remake volatile noalign
printf "snashot-unaligned\n"
for wrkld in $YCSB_WRKLD; do
    execute_snapshot
done


