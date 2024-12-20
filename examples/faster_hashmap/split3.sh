#!/bin/bash
sudo servant \
	--core 0 \
	--busypoll \
	--num-prog 3 \
	--batch-size 32 \
	$NET_IFACE 2 ./ubpf_three_phase.o

