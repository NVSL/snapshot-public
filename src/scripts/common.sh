export HUNDRED_K=100000
export MILLION=1000000
export TEN_MILLION=10000000

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000

export PMEM_LOC="${PEM_LOC:-/mnt/pmem0/}"
export LOG_LOC="${PMEM_LOC}cxlbuf_logs"


set -e
set -o pipefail
