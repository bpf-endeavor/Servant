#!/bin/bash
sudo servant \
	--core 0 \
	--busypoll \
	--num-prog 2 \
	--batch-size 32 \
	$NET_IFACE 2 ./ubpf_two_phase.o

