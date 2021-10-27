#include <stdint.h>
#include <arpa/inet.h>
#include "../../src/include/servant_engine.h"

// for packet parsing
#include <linux/if_ether.h> //ethhdr
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h> // IPPROTO_*

#include <bpf/bpf_endian.h>

#ifndef DUMP
#define DUMP(x, args...) { char fmt[] = x; ubpf_print((char *)fmt, ##args); } 
#endif

int bpf_prog(struct pktctx *ctx)
{
	/* int ret = 0; */

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
	// Swap MAC
	unsigned char tmp;
	for (int i = 0; i < 6; i++) {
		tmp = eth->h_source[i];
		eth->h_source[i] = eth->h_dest[i];
		eth->h_dest[i] = tmp;
	}
	if (eth->h_proto == htons(ETH_P_IP)) {
		struct iphdr *ip = (struct iphdr *)(eth + 1);
		// Swap IP
		DUMP("Src IP: %x\n", ntohl(ip->saddr));
		DUMP("Dst IP: %x\n", ntohl(ip->daddr));
		uint32_t tmp_ip = ip->saddr;
		ip->saddr = ip->daddr;
		ip->daddr = tmp_ip;
		if (ip->protocol == IPPROTO_UDP) {
			struct udphdr *udp = (ctx->data + (sizeof(*eth) + (ip->ihl * 4)));
			// struct udphdr *udp = (struct udphdr *)(ip + 1);
			DUMP("Transport: UDP\n");
			DUMP("Src PORT: %d\n", ntohs(udp->source));
			DUMP("Dst PORT: %d\n", ntohs(udp->dest));
			// Swap port
			uint16_t tmp_port = udp->source;
			udp->source = udp->dest;
			udp->dest = tmp_port;
		}
	}
	return SEND;
}
