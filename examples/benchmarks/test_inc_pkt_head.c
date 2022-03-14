#include "general_header.h"

#define FURTHER_PROCESSING 128
#define INCSIZE 32

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

/**
 * Print some information about the packet
 */
sinline void
dump_info(CONTEXT *ctx, struct ethhdr *eth)
{
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

SEC("prog")
/* Entry function */
#ifdef ISUBPF
	sinline
#endif
int bpf_prog(CONTEXT *ctx)
{
	int i;
	int ret;
	void* data;
	void* data_end;
	/* void *old_ptr; */
	struct ethhdr *eth;
	unsigned short pktsize = (long)ctx->data_end - (long)ctx->data;

#ifdef ISXDP
	if (bpf_xdp_adjust_head(ctx, 0 - INCSIZE)) {
		return XDP_DROP;
	}
#else
	ADJUST_HEAD_INCREASE(ctx, INCSIZE);
#endif

	data = (void *)(long)ctx->data;
	data_end = (void *)(long)ctx->data_end;
	/* old_ptr = data + INCSIZE; */
	/* ubpf_memmove(data, old_ptr, pktsize); */
	/* DUMP("psize: %d\n", pktsize); */

	/* swap every thing and send back */
	eth = data;
	if (out_of_pkt(eth, data_end))
		return XDP_DROP;
	swap_mac(eth);
	eth->h_proto = bpf_htons(ETH_P_IP);

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
			short curlen = bpf_ntohs(udp->len);
			char *payload_extra = (char *)(udp + 1) + curlen;
			udp->len = bpf_htons(curlen + INCSIZE);
			ip->tot_len = bpf_htons((long)data_end - (long)ip);
			for (int k = 0; k < INCSIZE; k++) {
				payload_extra[k] = (k % 10) + 0x30;
			}
		}
	}

	/* dump_info(ctx, eth); */
	INC_TPUT;
	return XDP_DROP;
	/* return XDP_TX; */
}
