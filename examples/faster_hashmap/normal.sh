#!/bin/bash
sudo servant \
	--core 0 \
	--num-prog 1 \
	--batch-size 16 \
	$NET_IFACE 2 ./ubpf.o

