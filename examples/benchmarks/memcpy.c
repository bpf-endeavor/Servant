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

SEC("prog")
/* Entry of XDP program */
#ifdef ISUBPF
sinline
#endif
int bpf_prog(CONTEXT *ctx)
{
	void* data = (void *)(long)ctx->data;
	void* data_end = (void *)(long)ctx->data_end;
	struct ethhdr *eth = NULL;
	struct iphdr *ip = NULL;
	struct udphdr *udp = NULL;
	int i = 0;
	int *value = NULL;
	const unsigned int zero = 0;

	eth = data;
	if (out_of_pkt(eth, data_end))
		return XDP_DROP;
	ip = (struct iphdr *)(eth + 1);
	if (out_of_pkt(ip, data_end))
		return XDP_DROP;
	udp = (struct udphdr *)(ip + 1);
	if (out_of_pkt(udp, data_end))
		return XDP_DROP;
	if (((void *)udp) + sizeof(*ip) > data_end)
		return XDP_DROP;

	for (i = 0; i < 64; i++) {
		memcpy(udp, ip, sizeof(*ip));
		memcpy(ip, udp, sizeof(*ip));
	}
	value = LOOKUP(tput, &zero);
	if (value)
		*value += 1;
	return XDP_TX;
}
