/**
 * Description:
 * This benchmark throughput of `bpf_tail_call` in XDP against
 * looping in the uBPF program.
 * */

#include "general_header.h"

#define REPEAT 20

#ifdef ISUBPF
sinline int bpf_prog(struct xdp_md *ctx);
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
#else
/* This map should be filled in the control plane (user app: loader) */
struct {
	__uint(type, BPF_MAP_TYPE_PROG_ARRAY);
	__type(key, unsigned int);
	__type(value, unsigned int);
	__uint(max_entries, 1);
	/* __uint(map_flags, BPF_F_MMAPABLE); */
} map_progs SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, unsigned int);
	__type(value, long);
	__uint(max_entries, 1);
	/* __uint(map_flags, BPF_F_MMAPABLE); */
} state SEC(".maps");

SEC("prog")
int bpf_prog(CONTEXT *ctx) {
	const int zero = 0;
	long *r = bpf_map_lookup_elem(&state, &zero);
	if (!r) {
		// should never happen. just for verifier.
		return XDP_DROP;
	}
	if (*r == REPEAT) {
		return XDP_DROP;
	} else {
		/* increment number of tail calls performed */
		*r += 1;
		bpf_tail_call(ctx, &map_progs, 0);
		// should not come here!
		return XDP_DROP;
	}
}
#endif

char _license[] SEC("license") = "GPL";
