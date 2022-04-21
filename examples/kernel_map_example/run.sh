#! /bin/bash

interface="enp24s0f1"
qid=0
core=0
servant=../../src/servant

sudo nice -n -20 taskset -c $core $servant --busypoll --xdp-prog ./xdp.o \
	--map xsks_map --map test_map --map data_map \
	--rx-size 512 --tx-size 512 --batch-size 64 \
	$interface $qid ./ubpf.o
