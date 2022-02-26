#include "../../src/include/servant_engine.h"

// for packet parsing
#include <linux/if_ether.h> //ethhdr
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h> // IPPROTO_*


#ifndef DUMP
#define DUMP(x, args...) { char fmt[] = x; ubpf_print((char *)fmt, ##args); } 
#endif

__attribute__((__always_inline__)) static inline
int bpf_prog(struct pktctx *ctx);

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

__attribute__((__always_inline__)) static inline
int bpf_prog(struct pktctx *ctx)
{
	struct ethhdr *eth = ctx->data;
	// Swap MAC
	unsigned char tmp;
	for (int i = 0; i < 6; i++) {
		tmp = eth->h_source[i];
		eth->h_source[i] = eth->h_dest[i];
		eth->h_dest[i] = tmp;
	}
	if (eth->h_proto == ubpf_htons(ETH_P_IP)) {
		struct iphdr *ip = (struct iphdr *)(eth + 1);
		// Swap IP
		uint32_t tmp_ip = ip->saddr;
		ip->saddr = ip->daddr;
		ip->daddr = tmp_ip;
		if (ip->protocol == IPPROTO_UDP) {
			struct udphdr *udp = (ctx->data + (sizeof(*eth) + (ip->ihl * 4)));
			// Swap port
			uint16_t tmp_port = udp->source;
			udp->source = udp->dest;
			udp->dest = tmp_port;
		}
	}
	return SEND;
}

