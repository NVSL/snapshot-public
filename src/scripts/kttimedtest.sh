#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="${DIR}/../../data"

LOG_F=`mktemp`
source ${DIR}/common.sh
ROOT="${DIR}/../../"

KYOTO_ROOT_PREFIX="${ROOT}/vendor/kyoto"
DB_LOC="/mnt/pmem0/db.kct"
DB_WC="/mnt/pmem0/db.*"
DB_DJ_LOC="/mnt/pmem0p2/db.kct"
DB_DJ_WC="/mnt/pmem0p2/db.*"
DB_MSYNC_LOC="/mnt/pmem0p3/db.kct"
DB_MSYNC_WC="/mnt/pmem0p3/db.*"
OPS=100000

set -e
set -o pipefail

run_ktycoon() {
    db_loc="$1"
    
    printf ","
    rm -f $DB_WC
    rm -f $DB_MSYNC_WC
    rm -f $DB_DJ_WC

    rm -rf "${LOG_LOC}"
    rm -rf /mnt/pmem0/*

    LD_LIBRARY_PATH="${KYOTO_ROOT}/kyototycoon:${KYOTO_ROOT}/kyotocabinet" \
                   "${KYOTO_ROOT}/kyototycoon/kttimedtest" tran -hard "$db_loc" $OPS 2>&1 \
        | tee -a "$LOG_F" \
        | grep time | sed 's|time: ||g' | tr -d '\n'
}

execute() {
    db_loc="$1"
    
    for tr in $TRAN_SIZES; do
        printf "%s" $(bc <<< "scale=3; 1/$tr")
        for i in 1 2 3 4 5 6; do
            TRAN_RND="$tr" run_ktycoon $db_loc | tee -a "$LOG_F"
        done
        printf "\n"
    done
}

TRAN_SIZES="1 2 4 8 16 32 64 128"
#TRAN_SIZES="1 2"

echo "Log in $LOG_F"

echo "=== msync ==="
KYOTO_ROOT="${KYOTO_ROOT_PREFIX}" execute "$DB_LOC" | tee "${DATA_DIR}/kyoto.msync.csv"

echo "=== cxlbuf ==="
CXLBUF_USE_HUGEPAGE=1 KYOTO_ROOT="${KYOTO_ROOT_PREFIX}.inst" CXL_MODE_ENABLED=1 execute "$DB_LOC" | tee "${DATA_DIR}/kyoto.cxlbuf.csv"

