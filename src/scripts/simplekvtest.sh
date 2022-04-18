#!/usr/bin/env bash

SIMPLEKV_ROOT="/home/smahar/git/cxlbuf/src/examples/"
CONFIG_FILE="${SIMPLEKV_ROOT}/../../make.config"
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
set -o pipefail
#set -x

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

    if [ "$V" = "1" ]; then 
        recompile
    else
        recompile >/dev/null 2>&1
    fi
}

execute() {
    rm -f "${PMDK_POOL}"*
    rm -f "${LOG_LOC}"

    pmempool create obj --layout=simplekv -s 1G "${PMDK_POOL}"
    
    "${SIMPLEKV_ROOT}/${SIMPLEKV_PMDK}" "${PMDK_POOL}" ycsb \
                                        "${YCSB_LOC}${wrkld}-load-1.0" \
                                        "${YCSB_LOC}${wrkld}-run-1.0"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf ","

    rm -f "${PMDK_POOL}"*
    rm -f "${LOG_LOC}"

    CXL_MODE_ENABLED=1 "${SIMPLEKV_ROOT}/${SIMPLEKV_CXLBUF}.inst" "${PMDK_POOL}" \
                       ycsb "${YCSB_LOC}${wrkld}-load-1.0" \
                       "${YCSB_LOC}${wrkld}-run-1.0"  2>&1 \
        | grep 'Total ns' | grep -Eo '[0-9]+' | tr -d '\n'

    printf ","

    rm -f "${PMDK_POOL}"*
    rm -f "${LOG_LOC}"

    printf "\n"
}

remake volatile
printf "pmdk,snashot,msync\n"
for wrkld in $YCSB_WRKLD; do
    execute
done


remake nonvolatile
printf "pmdk,snashot,msync\n"
for wrkld in $YCSB_WRKLD; do
    execute
done


