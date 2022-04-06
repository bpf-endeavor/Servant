#include <servant/servant_engine.h>
#include "internal_benchmarks.h"
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>

#define CHUNK_SIZE sizeof(struct iphdr)
#define REPEAT 256

int memcpy_bpf_prog(struct pktctx *ctx)
{
	void* data = (void *)(long)ctx->data;
	void* data_end = (void *)(long)ctx->data_end;
	struct ethhdr *eth = NULL;
	struct iphdr *ip = NULL;
	struct udphdr *udp = NULL;
	int i = 0;
	eth = data;
	ip = (struct iphdr *)(eth + 1);
	udp = (struct udphdr *)(ip + 1);
	if ((void *)(udp + 1) > data_end)
		return DROP;
	if (((void *)udp) + CHUNK_SIZE > data_end)
		return DROP;
	for (i = 0; i < REPEAT; i++) {
		memcpy(udp, ip, CHUNK_SIZE);
		udp->len += 1;
		memcpy(ip, udp, CHUNK_SIZE);
	}
	return DROP;
}