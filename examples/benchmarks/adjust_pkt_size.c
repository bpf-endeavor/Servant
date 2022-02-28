#include "general_header.h"

#define FURTHER_PROCESSING 128

sinline int bpf_prog(CONTEXT *ctx);

#ifdef ISUBPF
/**
 * Entry of the uBPF program
 */
int batch_processing_entry(struct pktctxbatch *batch)
{
	for (int i = 0; i < batch->cnt; i++) {
		batch->rets[i] = bpf_prog(&batch->pkts[i]);
	}
	return 0;
}
#endif

sinline void
swap_mac(struct ethhdr *eth)
{
	char tmp[6];
	memcpy(tmp, eth->h_dest, 6);
	memcpy(eth->h_dest, eth->h_source, 6);
	memcpy(eth->h_source, tmp, 6);
}

/* sinline void */
/* dump_info(CONTEXT *ctx, struct ethhdr *eth) */
/* { */
/* 	DUMP("Packet size: %ld\n", (uint64_t)(ctx->data_end - ctx->data)); */
/* 	DUMP("Src MAC: %x:%x:%x:", eth->h_source[0], eth->h_source[1], */
/* 			eth->h_source[2]); */
/* 	DUMP("%x:%x:%x\n", eth->h_source[3], eth->h_source[4], */
/* 			eth->h_source[5]); */
/* 	DUMP("Dest MAC: %x:%x:%x:", eth->h_dest[0], eth->h_dest[1], */
/* 			eth->h_dest[2]); */
/* 	DUMP("%x:%x:%x\n", eth->h_dest[3], eth->h_dest[4], */
/* 			eth->h_dest[5]); */
/* 	DUMP("Ether Type: %x\n", bpf_ntohs(eth->h_proto)); */
/* } */

sinline int
push_header(CONTEXT *ctx)
{
	void *data;
	void *data_end;
	struct ethhdr *new_eth;
	struct ethhdr *old_eth;
#ifdef ISXDP
	if (bpf_xdp_adjust_head(ctx, 0 - (int)sizeof(struct iphdr))) {
		return XDP_DROP;
	}
#else
	ADJUST_HEAD_INCREASE(ctx, sizeof(struct iphdr));
#endif
	data = (void*)(long)ctx->data;
	data_end = (void*)(long)ctx->data_end;
	new_eth = data;
	old_eth = data + sizeof(struct iphdr);
	if (out_of_pkt(new_eth, data_end) || out_of_pkt(old_eth, data_end)) {
		return XDP_DROP;
	}
	memcpy(new_eth, old_eth, sizeof(struct ethhdr));
	return FURTHER_PROCESSING;
}

SEC("prog")
/* Entry function */
sinline int bpf_prog(CONTEXT *ctx)
{
	int i;
	int ret;
	void* data;
	void* data_end;
	struct ethhdr *eth;

	for (i = 0; i < 8; i++) {
		ret = push_header(ctx);
		if (ret != FURTHER_PROCESSING)
			return ret;
	}

	data = (void *)(long)ctx->data;
	data_end = (void *)(long)ctx->data_end;
	eth = data;
	if (out_of_pkt(eth, data_end))
		return XDP_DROP;
	swap_mac(eth);
	eth->h_proto = bpf_htons(ETH_P_IP);
	/* dump_info(ctx, new_eth); */
	return XDP_TX;
}
