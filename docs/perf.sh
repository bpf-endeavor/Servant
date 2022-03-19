#! /bin/bash
perf_bin=./linux-5.13/tools/perf/perf
core=0
sudo  $perf_bin stat -B --cpu $core \
	-e cycles \
	-e cpu-clock \
	-e context-switches \
	-e cpu-migrations \
	-e instructions \
	-e cache-references \
	-e cache-misses \
	-r 3 sleep 1
