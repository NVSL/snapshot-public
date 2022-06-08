export HUNDRED_K=100000
export MILLION=1000000
export TEN_MILLION=10000000

export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000

export PMEM_LOC="${PEM_LOC:-/mnt/pmem0/}"
export LOG_LOC="${PMEM_LOC}cxlbuf_logs"

backtrace() {
    local deptn=${#FUNCNAME[@]}

    printf "Trapped on error, stacktrace:\n"
    
    for ((i=1; i<$deptn; i++)); do
        local func="${FUNCNAME[$i]}"
        local line="${BASH_LINENO[$((i-1))]}"
        local src="${BASH_SOURCE[$((i-1))]}"
        printf "\t%2d: " "$((i-1))"
        echo "$func(), $src, line $line"
    done

    if [ ! -z "$LOG_F" ]; then
        printf "\n=== Log written to $LOG_F ===\n\n"
    fi
}

function trace_top_caller() {
    local func="${FUNCNAME[1]}"
    local line="${BASH_LINENO[0]}"
    local src="${BASH_SOURCE[0]}"
    echo "  called from: $func(), $src, line $line"
}

# Check if the binary was compiled with a specific flag
check_bin() {
    BIN="$1"
    FLAG="$2"

    out=$(strings "$BIN" | grep -- "$FLAG" || true)

    if [ -z "$out" ]; then
        echo "Check failed for '$FLAG' in binary '$BIN'"
        exit 1
    fi

    return 0
}


set -eu
set -o pipefail

set -o errtrace
trap 'backtrace' ERR

