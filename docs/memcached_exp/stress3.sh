#!/bin/bash

ip=192.168.1.1
port=8080
conn=16
duration=60
# recs=10000000
recs=10000
general="-K fb_key -V fb_value -r $recs --popularity=zipf:1.25"
# general="-K 30 -V 100 -r $recs"
bin=./mutilate/mutilateudp
bin_loader=./mutilate/mutilate

if [ "x$1" = "xload" ]; then
	# Load the database
	$bin_loader -s $ip --loadonly $general
elif [ "x$1" = "xlow" ]; then
	# Low load for testing
	$bin -s $ip:$port -C 1 -c 1 --noload --time 5 $general &
	sleep 2
	# run a mutilate thread for updating values
	$bin_loader -s $ip --update 1 --time 5 $general
	sleep 3
else
	# Setup the Agent
	$bin -A -T 32 &
	sleep 1
	# Setup the Master
	$bin -s $ip:$port -C 1 -c $conn --noload -a 127.0.0.1 \
		--time $duration $general &

	sleep 2
	# run a mutilate thread for updating values
	# $bin_loader -c 16 -T 3 -q 50000 -s $ip --update 1 --time $duration $general
	$bin_loader -c 4 -T 1 -q 20000 -s $ip --update 1 --time $duration $general
	sleep 3
fi

pkill mutilate
pkill mutilateudp
