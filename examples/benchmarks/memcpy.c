/**
 * Description:
 * This benchmark test throughput of `memcpy` in XDP and uBPF virtual machine.
 * */

#include "general_header.h"

/* #define TEST_MYMEMCPY */

#define CHUNK_SIZE sizeof(struct iphdr)

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

#ifdef TEST_MYMEMCPY
sinline void mymemcpy(void *dst, void *src, unsigned short size)
{
	short i = 0;
	for (;i + sizeof(long) <= size;) {
		*(long*)(dst+i) = *(long*)(src+i);
		i += sizeof(long);
	}
	for (;i < size; i++) {
		*(char *)(dst+i) = *(char *)(src+i);
	}
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
	if (((void *)udp) + CHUNK_SIZE > data_end)
		return XDP_DROP;
#ifndef TEST_MYMEMCPY
	for (i = 0; i < 64; i++) {
		memcpy(udp, ip, CHUNK_SIZE);
		udp->len += 1;
		memcpy(ip, udp, CHUNK_SIZE);
	}
#else
	for (i = 0; i < 64; i++) {
		mymemcpy(udp, ip, CHUNK_SIZE);
		udp->len += 1;
		mymemcpy(ip, udp, CHUNK_SIZE);
	}
#endif
	value = LOOKUP(tput, &zero);
	if (value)
		*value += 1;
	return XDP_DROP;
}
