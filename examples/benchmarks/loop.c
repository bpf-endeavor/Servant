#include "general_header.h"

#define REPEAT 16

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

SEC("prog")
#ifdef ISUBPF
sinline
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
	unsigned int c = ip->tos + 1;
	for (int i = 0; i < REPEAT; i++) {
		c++;
		if (c % 7 == 0) {
			c &= 0xff;
			c *= 2;
		}
	}
	ip->tos = c;
	/* INC_TPUT; */
	return XDP_DROP;
}
