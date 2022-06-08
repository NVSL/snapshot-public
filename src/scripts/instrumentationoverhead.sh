#!/usr/bin/env bash

SIMPLEKV_ROOT="/home/smahar/git/cxlbuf/src/examples/"
CONFIG_FILE="${SIMPLEKV_ROOT}/../../make.config"
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

recompile() {
    make clean -C "$(dirname "$CONFIG_FILE")"
    make release -j$(nproc) -C "$(dirname "$CONFIG_FILE")"
}

remake() {
    if [ -z "$LLVM_DIR" ]; then
        echo "LLVM_DIR not set"
        exit 1
    fi
   
    if [ "$1" = "no_persist_ops" ]; then
        sed -i 's|NO_PERSIST_OPS=n|NO_PERSIST_OPS=y|g' "$CONFIG_FILE"
    else
        sed -i 's|NO_PERSIST_OPS=y|NO_PERSIST_OPS=n|g' "$CONFIG_FILE"
    fi

    if [ "$2" = "no_check_memory" ]; then
        sed -i 's|NO_CHECK_MEMORY=n|NO_CHECK_MEMORY=y|g' "$CONFIG_FILE"
    else
        sed -i 's|NO_CHECK_MEMORY=y|NO_CHECK_MEMORY=n|g' "$CONFIG_FILE"
    fi

    if [ "$V" = "1" ]; then 
        recompile
    else
        echo "Recompiling $(dirname "$CONFIG_FILE") ..." >&2
        recompile >/dev/null 2>&1
    fi
}

execute() {
    BIN="$1"

    "${BIN}" "${PMDK_POOL}" ycsb "${YCSB_LOC}${wrkld}-load-1.0" \
             "${YCSB_LOC}${wrkld}-run-1.0" 2>&1
}

run() {
    delete_files
    remake persist_ops check_memory

    CXL_MODE_ENABLED=1 execute "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}" \
        | grep -m1 'ns = ' | head -n1 | grep -Eo '[0-9]+' | tr -d '\n' 
    printf ","

    delete_files
    remake no_persist_ops check_memory

    CXL_MODE_ENABLED=1 execute "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}" \
        | grep -m1 'ns = ' | head -n1 | grep -Eo '[0-9]+' | tr -d '\n' 
    printf ","

    delete_files
    remake persist_ops no_check_memory

    CXL_MODE_ENABLED=1 execute "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}" \
        | grep -m1 'ns = ' | head -n1 | grep -Eo '[0-9]+' | tr -d '\n' 
    printf "," 

    delete_files
    DISABLE_PASS=1 remake persist_ops no_check_memory
    CXL_MODE_ENABLED=1 execute "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}" \
        | grep -m1 'ns = ' | head -n1 | grep -Eo '[0-9]+' | tr -d '\n' 
    printf "\n"

}

for i in 1 2 3 4 5; do
    printf "workload,baseline,no persist ops,no check memory,no instrumentation\n"
    for wrkld in $YCSB_WRKLD; do
        printf "${wrkld},"
        run
    done
done

