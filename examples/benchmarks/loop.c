#include "general_header.h"
#ifdef ISUBPF
sinline int bpf_prog(CONTEXT *ctx);
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

int bpf_prog(CONTEXT *ctx)
{
	void *data = (void *)(long)ctx->data;
	void *data_end = (void *)(long)ctx->data_end;
	struct ethhdr *eth = data;
	if (out_of_pkt(eth, data_end))
		return XDP_DROP;
	struct iphdr *ip = (struct iphdr *)(eth + 1);
	if (out_of_pkt(ip, data_end))
		return XDP_DROP;
	unsigned int c = (unsigned int)(long)ctx->data;
	for (int i = 0; i < 2048; i++) {
		c++;
	}
	ip->tos = c % 8;
	INC_TPUT;
	return XDP_DROP;
}
