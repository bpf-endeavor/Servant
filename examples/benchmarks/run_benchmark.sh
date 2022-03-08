#!/bin/bash

device=enp24s0f1

curdir=`dirname $0`
curdir=`realpath $curdir`
loader="$curdir/bin/loader"
servant="$curdir/../../src/servant"
queue=0

usage() {
	echo "$0 <exp_mode> <binary_object>"
	echo "$0 state_overhead <state_size> <binary_object>"
	echo "--------------------------------------"
	echo "* exp_mode: xdp | ubpf"
}

run_xdp() {
	if [ -f $1 ]; then
		rm $1
	fi
	make XDP=1
	sudo $loader -N -F $device $1
}

run_ubpf() {
	rm $curdir/bin/xdp.o
	if [ -f $1 ]; then
		rm $1
	fi
	make UBPF=1
	sudo taskset -c $queue $servant --busypoll --xdp-prog "$curdir/bin/xdp.o" \
		--map xsks_map --map tput \
		--rx-size 4096 --tx-size 4096 \
		--batch-size 64 \
		$device $queue $1
}

run_state_overhead_test() {
	rm $curdir/bin/xdp.o
	if [ -f $2 ]; then
		rm $2
	fi
	make UBPF=1 COPY_STATE=$1
	sleep 1
	sudo taskset -c $queue $servant --busypoll --xdp-prog "$curdir/bin/xdp.o" \
		--map xsks_map --map tput \
		--rx-size 4096 --tx-size 4096 \
		--batch-size 64 \
		$device $queue $2
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
elif [ $mode = state_overhead ]; then
	if [ $# -lt 3 ]; then
		usage
		exit 1
	fi
	run_state_overhead_test $2 $3
else
	usage
	exit 1
fi
