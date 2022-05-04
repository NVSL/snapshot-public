#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIR}/common.sh

ROOT="${DIR}/../.."
REDIS_ROOT_PREFIX="${ROOT}/vendor/redis/src"
REDIS_INST_ROOT_PREFIX="${ROOT}/vendor/redis.inst/src"
DB_WC="/mnt/pmem0/redis.pm*"
OPS=100000

run_desock_redis() {
#    LD_PRELOAD="${ROOT}"/vendor/preeny/src/desock.so ltrace -c -o /tmp/vanilla.ltrace "${REDIS_ROOT_PREFIX}"/redis-server "${REDIS_ROOT_PREFIX}"/../redis.conf
#    LD_PRELOAD="${ROOT}"/vendor/preeny/src/desock.so strace -o /tmp/vanilla.strace "${REDIS_ROOT_PREFIX}"/redis-server "${REDIS_ROOT_PREFIX}"/../redis.conf
    LD_PRELOAD="${ROOT}"/vendor/preeny/src/desock.so "${REDIS_ROOT_PREFIX}"/redis-server "${REDIS_ROOT_PREFIX}"/../redis.conf
}

run_desock_redis_inst() {
#    LD_PRELOAD="${ROOT}"/vendor/preeny/src/desock.so ltrace -c -o /tmp/cxlbuf.ltrace "${REDIS_INST_ROOT_PREFIX}"/redis-server "${REDIS_INST_ROOT_PREFIX}"/../redis.conf
#    LD_PRELOAD="${ROOT}"/vendor/preeny/src/desock.so strace -o /tmp/cxlbuf.strace "${REDIS_INST_ROOT_PREFIX}"/redis-server "${REDIS_INST_ROOT_PREFIX}"/../redis.conf
    CXL_MODE_ENABLED=1 LD_PRELOAD="${ROOT}"/vendor/preeny/src/desock.so "${REDIS_INST_ROOT_PREFIX}"/redis-server "${REDIS_INST_ROOT_PREFIX}"/../redis.conf
}

execute() {
    wrkld="$1"

    printf ","
    rm -f $DB_WC
    rm -rf "${LOG_LOC}"

    pkill redis-server || { :; }

    printf "$(cat ~/git/libpuddles-scripts/traces/redis/$wrkld-load-1.0)\nshutdown\n"  \
        | run_desock_redis 2>&1 | grep -i 'time' | tail -n1

    rm -f $DB_WC
    rm -rf "${LOG_LOC}"

    pkill redis-server || { :; }

    printf "$(cat ~/git/libpuddles-scripts/traces/redis/$wrkld-load-1.0)\nshutdown\n"  \
        | run_desock_redis_inst 2>&1 | grep -i 'time'

}

WRKLDS="a"

for wrkld in $WRKLDS; do
    execute "$wrkld"

    printf "\n"
done
