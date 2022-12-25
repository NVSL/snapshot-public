#!/usr/bin/env bash

export DISABLE_SAFEGUARDS=1

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh
ROOT="${DIR}/../../"

V="${V:-0}"
LOG_F="$(mktemp)"
SIMPLEKV_ROOT="${ROOT}/src/examples/"
CONFIG_FILE="${SIMPLEKV_ROOT}/../../make.config"
SIMPLEKV_CXLBUF="simplekv_cxlbuf/simplekv_puddles"
SIMPLEKV_PMDK="simplekv_pmdk/simplekv_pmdk"
SIMPLEKV_NOSYNC="simplekv_cxlbuf/simplekv_cxlbuf"
YCSB_LOC="${ROOT}/data/traces/"
PMDK_POOL="/mnt/pmem0/simplekv"
NUMA_CTL="numactl -N0 --"

YCSB_WRKLD="a b c d e f g"

OPS=100000

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=0

set +e
# set -x

delete_files() {
    rm -f "${PMDK_POOL}"*
    rm -rf "${LOG_LOC}"
}

recompile() {
    make clean -C "$(dirname "$CONFIG_FILE")"
    make release -j$(nproc --all) -C "$(dirname "$CONFIG_FILE")"
}

remake() {
    if [ -z "$LLVM_DIR" ]; then
        echo "LLVM_DIR not set"
        exit 1
    fi
   
    sed -i 's|CXLBUF_TESTING_GOODIES=n|CXLBUF_TESTING_GOODIES=y|g' "$CONFIG_FILE"

    if [ "$V" = "1" ]; then 
        recompile
    else
        echo "Recompiling $(dirname "$CONFIG_FILE") ..." >&2
        recompile >/dev/null 2>&1
    fi
}

execute() {
    BIN="$1"

    ${NUMA_CTL} "${BIN}" "${PMDK_POOL}" ycsb "${YCSB_LOC}${wrkld}.load" \
                "${YCSB_LOC}${wrkld}.run" 2>&1
}

run() {
    delete_files
 
    CXL_MODE_ENABLED=0 execute "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}" | tee -a "${LOG_F}"\
        | grep -E 'perst_overhead|(Total ns)' | grep -Eo '[0-9]+' | tr '\n' '/' | sed 's|/$||g' | xargs printf "scale=2; 100/(%s)\n" | bc | tr -d '\n'
}

remake

printf "workload,overhead\n"
for wrkld in $YCSB_WRKLD; do
    printf "${wrkld},"
    CXLBUF_USE_HUGEPAGE=0 run
    printf "\n"
done

printf "workload,overhead\n"
for wrkld in $YCSB_WRKLD; do
    printf "${wrkld},"
    CXLBUF_USE_HUGEPAGE=1 run
    printf "\n"
done


