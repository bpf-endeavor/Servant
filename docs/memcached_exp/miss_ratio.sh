#! /bin/bash

cat /tmp/bmc_stats.txt | awk 'BEGIN {miss=0; hit=0;} /hit_count/ {hit=$3;} /miss_count/ {miss=$3;} END {print miss * 100 / (miss + hit)}'

echo ...........................
cat /tmp/bmc_stats.txt

