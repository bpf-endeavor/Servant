#!/bin/bash

if [ -z $DEVICE ]; then
device=enp24s0f1
else
device=$DEVICE
fi

curdir=`dirname $0`
curdir=`realpath $curdir`
loader="$curdir/bin/loader"
servant="$curdir/../../src/servant"
queue=0
ring_size=512

usage() {
	echo "$0 <exp_mode> <binary_object>"
	echo "$0 state_overhead <state_size> <binary_object>"
	echo "--------------------------------------"
	echo "* exp_mode: xdp | ubpf"
	echo "* binary_object: name of binary object (inside ./bin/ directory)"
}

turn_off_busypolling() {
	echo 0 | sudo tee /sys/class/net/$device/napi_defer_hard_irqs
	echo 0 | sudo tee /sys/class/net/$device/gro_flush_timeout
}

busypoll_budget=200000
turn_on_busypolling() {
	echo 2 | sudo tee /sys/class/net/$device/napi_defer_hard_irqs
	echo $busypoll_budget | sudo tee /sys/class/net/$device/gro_flush_timeout
}

run_xdp() {
	turn_off_busypolling
	if [ -f $1 ]; then
		rm $1
	fi
	make XDP=1
	sudo $loader -N -F $device $1
}

run_ubpf() {
	turn_on_busypolling
	rm $curdir/bin/xdp.o
	if [ -f $1 ]; then
		rm $1
	fi
	make UBPF=1
	sudo taskset -c $queue $servant --busypoll $copy_flag \
		--xdp-prog "$curdir/bin/xdp.o" \
		--map xsks_map --rx-size $ring_size --tx-size $ring_size \
		--batch-size 64 \
		$device $queue $1
}

run_state_overhead_test() {
	turn_on_busypolling
	rm $curdir/bin/xdp.o
	if [ -f $2 ]; then
		rm $2
	fi
	make UBPF=1 COPY_STATE=$1
	sleep 1
	sudo taskset -c $queue $servant --busypoll --xdp-prog "$curdir/bin/xdp.o" \
		--map xsks_map --map tput \
		--rx-size $ring_size --tx-size $ring_size \
		--batch-size 64 \
		$device $queue $2
}

if [ $# -lt 2 ]; then
	usage
	exit 1
fi


mode=$1
binobj="$curdir/bin/$2"
copy_flag="--copy"
if [ $mode = xdp ]; then
	run_xdp $binobj
elif [ $mode = ubpf ]; then
	run_ubpf $binobj
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
