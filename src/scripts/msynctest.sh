#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh


EX_ROOT="/home/smahar/git/cxlbuf/src/examples"

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXL_MODE_ENABLED=1
export CXLBUF_USE_HUGEPAGE=1
set -e

LOG_F=/tmp/output.log

FS_DJ_POOL="/mnt/pmem0p2/simplekv"
FS_POOL="/mnt/pmem0p3/simplekv"

clean() {
    rm -f "$FS_DJ_POOL"*
    rm -f "$FS_POOL"*
}

clean
BENCHMARK_FILE="${FS_POOL}" CXLBUF_USE_HUGEPAGE=1 "${EX_ROOT}/microbenchmarks/run" --run=msyncscaling | tee "${EX_ROOT}/../../data/msyncscaling_hugepage.csv"
clean
BENCHMARK_FILE="${FS_POOL}" CXLBUF_USE_HUGEPAGE=0 "${EX_ROOT}/microbenchmarks/run" --run=msyncscaling | tee "${EX_ROOT}/../../data/msyncscaling.csv"
clean
BENCHMARK_FILE="${FS_DJ_POOL}" CXLBUF_USE_HUGEPAGE=0 "${EX_ROOT}/microbenchmarks/run" --run=msyncscaling | tee "${EX_ROOT}/../../data/msyncscaling_dj.csv"
clean
BENCHMARK_FILE="${FS_POOL}" "${EX_ROOT}/microbenchmarks_cxlbuf/run" --run=msyncscaling | tee "${EX_ROOT}/../../data/msyncscaling_cxlbuf.csv"


