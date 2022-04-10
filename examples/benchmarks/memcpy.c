/**
 * Description:
 * This benchmark test throughput of `memcpy` in XDP and uBPF virtual machine.
 * */
#include "general_header.h"

#define USE_TX
#define CHUNK_SIZE sizeof(struct iphdr)
#define REPEAT 64

SEC("prog")
int bpf_prog(CONTEXT *ctx)
{
	void *data = (void *)(long)ctx->data;
	void *data_end = (void *)(long)ctx->data_end;
	struct ethhdr *eth = NULL;
	struct iphdr *ip = NULL;
	struct udphdr *udp = NULL;
	int i = 0;
	eth = data;
	ip = (struct iphdr *)(eth + 1);
	udp = (struct udphdr *)(ip + 1);
	if (out_of_pkt(udp, data_end))
		return XDP_DROP;
	if (((void *)udp) + CHUNK_SIZE > data_end)
		return XDP_DROP;
	for (i = 0; i < REPEAT; i++) {
		memcpy(udp, ip, CHUNK_SIZE);
		udp->len += 1;
		memcpy(ip, udp, CHUNK_SIZE);
	}
#ifdef ISXDP
	INC_TPUT;
#endif
#ifndef USE_TX
	return XDP_DROP;
#else
	// Swap MAC
	unsigned char tmp;
	for (int i = 0; i < 6; i++) {
		tmp = eth->h_source[i];
		eth->h_source[i] = eth->h_dest[i];
		eth->h_dest[i] = tmp;
	}
	return XDP_TX;
#endif
}
