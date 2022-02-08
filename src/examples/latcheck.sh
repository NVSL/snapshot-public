#!/usr/bin/env bash

set -e

CAT_TOOLS_LOC="/home/smahar/git/intel-cmt-cat/examples/c/CAT_MBA"
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
TGT_BIN="${SCRIPT_DIR}/cat-demo"
TGT_CPU="0"

SUDO=sudo

source "${SCRIPT_DIR}/cattools.sh"

fatal() {
    printf "ERROR: "
    printf "$@"
    printf "\n"
    exit 1
}

run() {
    result=$("$TGT_BIN" "$TGT_CPU")

    line0=$(echo "$result" | head -n1)
    line1=$(echo "$result" | tail -n1)

    echo -e "$line1"
}

if [ "${NO_EXECUTE:-0}" -eq 1 ]; then
    return
fi

if [ ! -e "$TGT_BIN" ]; then
    fatal "$TGT_BIN does not exist."
fi

printf "no CAT,"
reset
run


printf "CAT exclusive 0x00f,"
reset
set_associations
set_mask 0 0x7f0
set_mask 1 0x00f
run

printf "CAT exclusive 0x001,"
reset
set_associations
set_mask 0 0x7fe
set_mask 1 0x001
run


printf "CAT shared tiny,"
reset
set_associations
set_mask 0 0x001
set_mask 1 0x001
run
