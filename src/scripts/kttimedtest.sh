#!/usr/bin/env bash

KYOTO_ROOT="/home/smahar/git/kyoto/"
DB_LOC="/mnt/pmem0/db.kct"
DB_WC="/mnt/pmem0/db.*"
OPS=100000

export LD_LIBRARY_PATH="${KYOTO_ROOT}/kyototycoon:${KYOTO_ROOT}/kyotocabinet"

execute() {
    printf ","
    rm -f $DB_WC
    "${KYOTO_ROOT}kyototycoon/kttimedtest" tran -hard "$DB_LOC" $OPS\
        | grep time | sed 's|time: ||g' | tr -d '\n'
}

for tr in 1 2 4 8 16 32 64 128; do
    printf "%s" $(bc <<< "scale=2; 1/$tr")

    for i in 1 2 3 4 5 6; do
        TRAN_RND="$tr" execute
    done
    for i in 1 2 3 4 5 6; do
        TRAN_RND="$tr" CXL_MODE_ENABLED=1 execute
    done
    
    printf "\n"
done
