#!/usr/bin/env sh

rm src/examples/periodic-msync.csv

echo 'update count,msync,memcpy and flush+sfence' > src/examples/periodic-msync.csv

for i in 256 1024 2048 4096 8192 16384 32768; do
	src/examples/periodic-msync /mnt/pmem0/periodic-msync /mnt/pmem0/periodic-msync-backing $i | tee -a src/examples/periodic-msync.csv;
done
