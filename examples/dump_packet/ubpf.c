#include <stdint.h>
#include "../../src/include/servant_engine.h"

// for packet parsing
#include <linux/if_ether.h> //ethhdr

#include <bpf/bpf_endian.h>

#ifndef DUMP
#define DUMP(x, args...) { char fmt[] = x; ubpf_print((char *)fmt, ##args); } 
#endif

int bpf_prog(struct pktctx *ctx)
{
	int ret = 0;

	struct ethhdr *eth = ctx->data;
	DUMP("Packet size: %ld\n", (uint64_t)(ctx->data_end - ctx->data));
	DUMP("Src MAC: %x:%x:%x:", eth->h_source[0], eth->h_source[1],
			eth->h_source[2]);
	DUMP("%x:%x:%x\n", eth->h_source[3], eth->h_source[4],
			eth->h_source[5]);
	DUMP("Dest MAC: %x:%x:%x:", eth->h_dest[0], eth->h_dest[1],
			eth->h_dest[2]);
	DUMP("%x:%x:%x\n", eth->h_dest[3], eth->h_dest[4],
			eth->h_dest[5]);
	DUMP("Ether Type: %x\n", bpf_ntohs(eth->h_proto));
	/* struct ethhdr = ctx->data; */
	return ret;
}
