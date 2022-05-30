#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh

KYOTO_ROOT_PREFIX="/home/smahar/git/cxlbuf/vendor/kyoto"
DB_LOC="/mnt/pmem0/db.kct"
DB_WC="/mnt/pmem0/db.*"
OPS=100000



execute() {
    printf ","
    rm -f $DB_WC
    rm -rf "${LOG_LOC}"

    LD_LIBRARY_PATH="${KYOTO_ROOT}/kyototycoon:${KYOTO_ROOT}/kyotocabinet" \
                   "${KYOTO_ROOT}/kyototycoon/kttimedtest" tran -hard "$DB_LOC" $OPS 2>&1 \
        | grep time | sed 's|time: ||g' | tr -d '\n'
}

TRAN_SIZES="1 2 4 8 16 32 64 128"
#TRAN_SIZES="1 2"

for tr in $TRAN_SIZES; do
    printf "%s" $(bc <<< "scale=3; 1/$tr")

    for i in 1 2 3 4 5 6; do
        KYOTO_ROOT="${KYOTO_ROOT_PREFIX}" TRAN_RND="$tr" execute
    done
    for i in 1 2 3 4 5 6; do
        KYOTO_ROOT="${KYOTO_ROOT_PREFIX}.inst" TRAN_RND="$tr" CXL_MODE_ENABLED=1 execute
    done
    
    printf "\n"
done
