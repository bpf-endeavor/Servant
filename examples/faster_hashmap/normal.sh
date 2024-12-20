#!/bin/bash
sudo servant \
	--core 0 \
	--busypoll \
	--num-prog 1 \
	--batch-size 32 \
	$NET_IFACE 2 ./ubpf.o

