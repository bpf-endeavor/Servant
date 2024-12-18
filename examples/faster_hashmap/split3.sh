#!/bin/bash
sudo servant \
	--core 0 \
	--num-prog 3 \
	--batch-size 16 \
	$NET_IFACE 2 ./ubpf_three_phase.o

