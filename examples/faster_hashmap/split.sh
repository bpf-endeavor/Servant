#!/bin/bash
sudo servant \
	--core 0 \
	--num-prog 2 \
	--batch-size 16 \
	$NET_IFACE 2 ./ubpf_two_phase.o

