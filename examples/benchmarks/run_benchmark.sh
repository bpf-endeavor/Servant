#!/bin/bash

device=enp24s0f1

curdir=`dirname $0`
curdir=`realpath $curdir`
loader="$curdir/bin/loader"
servant="$curdir/../../src/servant"

usage() {
	echo "$0 <exp_mode> <binary_object>"
	echo "* exp_mode: xdp | ubpf"
}

run_xdp() {
	make XDP=1
	sudo $loader -N -F $device $1
}

run_ubpf() {
	make UBPF=1
	sudo taskset -c 2 $servant --busypoll --xdp-prog "$curdir/bin/xdp.o" \
		--map xsks_map --map tput \
		--rx-size 4096 --tx-size 4096 \
		--batch-size 64 \
		$device 2 $1
}

if [ $# -lt 2 ]; then
	usage
	exit 1
fi

mode=$1
if [ $mode = xdp ]; then
	run_xdp $2
elif [ $mode = ubpf ]; then
	run_ubpf $2
else
	usage
	exit 1
fi
