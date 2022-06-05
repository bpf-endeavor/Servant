#!/bin/bash

# SERVER_IP is an environment variable of the container
ip=$SERVER_IP
port=8080
conn=16
duration=60
recs=10000000
# recs=10000
general="-K fb_key -V fb_value -r $recs --popularity=zipf:1.25"
# general="-K 30 -V 100 -r $recs"
bin=./mutilate/mutilateudp
bin_loader=./mutilate/mutilate

if [ "x$1" = "xload" ]; then
	# Load the database
	$bin_loader -s $ip --loadonly $general
elif [ "x$1" = "xlow" ]; then
	# Low load for testing
	$bin -s $ip:$port -C 1 -c 1 --noload --time 5 $general
else
	# Setup the Agent
	$bin -A -T 32 &
	sleep 1
	# Setup the Master
	$bin -s $ip:$port -C 1 -c $conn --noload -a 127.0.0.1 \
		--time $duration $general
fi

pkill mutilate
pkill mutilateudp
