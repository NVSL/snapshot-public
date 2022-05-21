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

CXLBUF_USE_HUGEPAGE=1 "${EX_ROOT}/microbenchmarks/run" --run=msyncscaling | tee "${EX_ROOT}/../../data/msyncscaling_hugepage.csv"
CXLBUF_USE_HUGEPAGE=0 "${EX_ROOT}/microbenchmarks/run" --run=msyncscaling | tee "${EX_ROOT}/../../data/msyncscaling.csv"
"${EX_ROOT}/microbenchmarks_cxlbuf/run" --run=msyncscaling | tee "${EX_ROOT}/../../data/msyncscaling_cxlbuf.csv"


