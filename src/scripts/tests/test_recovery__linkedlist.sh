#!/usr/bin/env sh

export CXL_MODE_ENABLED=1
LOG_F=`mktemp`

printf "Writing logs to ${LOG_F}"

if [ -z "$CXLBUF_RUNNING_TESTS" ]; then
    echo "Do not run individual test files directly. Use run.sh" 1>&2
    exit 1
fi

trl_remake() {
    clear_config >/dev/null 2>&1
    set_env CXLBUF_TESTING_GOODIES=y
    recompile >/dev/null 2>&1    
}

test_recovery_linkedlist() {
    trl_remake
    
    BIN="${ROOT_DIR}/src/examples/linkedlist_tx/linkedlist_tx"
    POOL="${PMEM_LOC}/linkedlist"
    rm -rf "$LOG_LOC" "$POOL"*

    printf "> Creating a pool\n"
    echo "i 1" | "$BIN" "$POOL" 2>/dev/null 1>&2

    printf "> Crashing on insertion\n"
    echo "p\ni 2" | CXLBUF_CRASH_ON_COMMIT=1  "$BIN" "$POOL" 2>/dev/null 1>&2

    printf "> Recovering\n"
    recOut=$(echo "p" | "$BIN" "$POOL" 2>/dev/null)

    recMatch=`echo "$recOut" | grep -Eo '\\\$ 1 \$'`
    assertEquals '$ 1 ' "${recMatch}"
}
suite_addTest test_recovery_linkedlist


test_recovery_linkedlist_multiple() {
    trl_remake

    rm -rf "$LOG_LOC" "${PMEM_LOC}/"linkedlist*

    printf "> Creating a pool\n" | tee -a "${LOG_F}"
    echo "i 1\ni 2\ni 3" | "$BIN" "$POOL" | tee -a "${LOG_F}" 2>/dev/null 1>&2

    printf "> Crashing on insertion\n" | tee -a "${LOG_F}"
    echo "i 10" | CXLBUF_CRASH_ON_COMMIT=1  "$BIN" "$POOL" | tee -a "${LOG_F}" 2>/dev/null 1>&2

    printf "> Recovering\n"
    recOut=$(echo "p" | "$BIN" "$POOL" 2>/dev/null)
    printf "recOut = \n====\n${recOut}\n====\n"
    recMatch=`echo "$recOut" | grep -Eo '\\\$ 2 3 1 \$'`

    assertEquals '$ 2 3 1 ' "${recMatch}"
}
suite_addTest test_recovery_linkedlist_multiple
