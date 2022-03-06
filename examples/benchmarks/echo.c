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
	// Swap MAC
	unsigned char tmp;
	for (int i = 0; i < 6; i++) {
		tmp = eth->h_source[i];
		eth->h_source[i] = eth->h_dest[i];
		eth->h_dest[i] = tmp;
	}
	if (eth->h_proto == bpf_htons(ETH_P_IP)) {
		struct iphdr *ip = (struct iphdr *)(eth + 1);
		if (out_of_pkt(ip, data_end))
			return XDP_DROP;
		// Swap IP
		unsigned int tmp_ip = ip->saddr;
		ip->saddr = ip->daddr;
		ip->daddr = tmp_ip;
		if (ip->protocol == IPPROTO_UDP) {
			struct udphdr *udp = (data + (sizeof(*eth) + (ip->ihl * 4)));
			if (out_of_pkt(udp, data_end))
				return XDP_DROP;
			// Swap port
			unsigned short tmp_port = udp->source;
			udp->source = udp->dest;
			udp->dest = tmp_port;
		}
	}
	return XDP_TX;
}
