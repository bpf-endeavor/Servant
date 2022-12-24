#include "../../src/include/servant_engine.h"

// for packet parsing
#include <linux/if_ether.h> //ethhdr
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h> // IPPROTO_*


#ifndef DUMP
#define DUMP(x, args...) { char fmt[] = x; ubpf_print((char *)fmt, ##args); }
#endif

#define delay_cycles 0

// #define SEND

int bpf_prog(struct pktctx *ctx)
{

#ifdef SEND
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
#endif

	uint64_t begin = ubpf_rdtsc();
	uint64_t now = begin;
	uint64_t end = now + delay_cycles;
	if (now <= end) {
		while (now  < end) {
			now = ubpf_rdtsc();
		}
	} else {
		while (now > end) {
			now = ubpf_rdtsc();
		}
		while (now < end) {
			now = ubpf_rdtsc();
		}
	}
#ifdef SEND
	return SEND;
#else
	return DROP;
#endif
}

