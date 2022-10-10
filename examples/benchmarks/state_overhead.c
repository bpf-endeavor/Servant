/* This program should only be used in uBPF engine */
#include "general_header.h"

#ifndef COPY_STATE
#define COPY_STATE 32
#endif

struct on_packet_state {
	char state[COPY_STATE];
} __attribute__((packed));

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

sinline
int bpf_prog(CONTEXT *ctx)
{
	void * data = (void *)(long)ctx->data;
	void * data_end = (void *)(long)ctx->data_end;
	struct on_packet_state *pkt_state = data_end - sizeof(struct on_packet_state);
	struct on_packet_state state;
	/* memcpy(&state, pkt_state, sizeof(struct on_packet_state)); */
	mymemcpy(&state, pkt_state, sizeof(struct on_packet_state));
	INC_TPUT;
	return XDP_DROP;
}
#endif
