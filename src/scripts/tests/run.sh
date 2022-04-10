export PMEM_START_ADDR=0x10000000000
export PMEM_END_ADDR=0x20000000000
export CXLBUF_RUNNING_TESTS=1

DIR="$(dirname $(readlink -f "$0"))"
ROOT_DIR="$(readlink -f "$DIR/../../../")"
PMEM_LOC="${PMEM_LOC:-/mnt/pmem0/}"
LOG_LOC="${PMEM_LOC}/cxlbuf_logs"

beginswith() { case $2 in "$1"*) true;; *) false;; esac; }

echo "# Found following tests file: "
suite() {
    for f in "$DIR"/test_*; do
        echo "#   $f"
        . "$f"
    done
}

echo '--------------'
echo 'Starting tests'
echo '--------------'

. "$DIR/../../../vendor/shunit2"
