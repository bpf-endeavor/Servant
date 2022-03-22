#include "general_header.h"
#define PAYLOAD_SIZE 64

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
	bpf_xdp_adjust_tail(ctx, PAYLOAD_SIZE);
	// 256 bytes
	char str[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit,"
		"sed do eiusmod tempor incididunt ut labore et dolore magna"
		"aliqua. Ut tellus elementum sagittis vitae. Eros donec ac odio"
		"tempor orci dapibus ultrices in. Egestas erat imperdiet sed"
		"euismod nisi port";
	void *data = (void *)(long)ctx->data;
	void *data_end = (void *)(long)ctx->data_end;
	struct ethhdr *eth = data;
	struct iphdr *ip = (struct iphdr *)(eth + 1);
	struct udphdr *udp = (struct udphdr *)(ip + 1);
	char *payload = (char *)(udp + 1);
	if ((void *)(payload + PAYLOAD_SIZE) > data_end) {
		return XDP_DROP;
	}
	memcpy(payload, str, PAYLOAD_SIZE);
	/* INC_TPUT; */
	return XDP_DROP;
}
