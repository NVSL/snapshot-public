#!/usr/bin/env sh

export CXL_MODE_ENABLED=1

if [ -z "$CXLBUF_RUNNING_TESTS" ]; then
    echo "Do not run individual test files directly. Use run.sh" 1>&2
    exit 1
fi

test_recovery_linkedlist() {
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
    rm -rf "$LOG_LOC" "${PMEM_LOC}/"linkedlist*

    printf "> Creating a pool\n"
    echo "i 1\ni 2\ni 3" | "$BIN" "$POOL" 2>/dev/null 1>&2

    printf "> Crashing on insertion\n"
    echo "i 10" | CXLBUF_CRASH_ON_COMMIT=1  "$BIN" "$POOL" 2>/dev/null 1>&2

    printf "> Recovering\n"
    recOut=$(echo "p" | "$BIN" "$POOL" 2>/dev/null)
    recMatch=`echo "$recOut" | grep -Eo '\\\$ 2 3 1 \$'`

    assertEquals '$ 2 3 1 ' "${recMatch}"
}
suite_addTest test_recovery_linkedlist_multiple
