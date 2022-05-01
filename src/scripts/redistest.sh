#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh

ROOT="${DIR}/../.."
REDIS_ROOT_PREFIX="${ROOT}/vendor/redis/src"
DB_WC="/mnt/pmem0/redis.pm*"
OPS=100000

run_desock_redis() {
    LD_PRELOAD="${ROOT}"/vendor/preeny/src/desock.so "${REDIS_ROOT_PREFIX}"/redis-server "${REDIS_ROOT_PREFIX}"/../redis.conf
}

execute() {
    wrkld="$1"

    printf ","
    rm -f $DB_WC
    rm -rf "${LOG_LOC}"

    pkill redis-server || { :; }

    printf "$(cat ~/git/libpuddles-scripts/traces/redis/$wrkld-run-1.0)\nshutdown\n"  \
        | run_desock_redis 
}

WRKLDS="a"

for wrkld in $WRKLDS; do
    CXL_MODE_ENABLED=1 execute "$wrkld"

    printf "\n"
done
