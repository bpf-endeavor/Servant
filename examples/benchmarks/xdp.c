/* This is XDP program used for testing uBPF benchmarks */
#include <string.h>
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#define FAIL_PTR_BOUND(s, end) (s + 1) > (void *)end

#ifdef COPY_STATE
/* This struct represents the state that is shared with uBPF */
struct on_packet_state {
	char state[COPY_STATE];
} __attribute__((packed));
#endif

/* Socket map for sending packets to userspace */
struct bpf_map_def SEC("maps") xsks_map = {
	.type = BPF_MAP_TYPE_XSKMAP,
	.key_size = sizeof(int),
	.value_size = sizeof(int),
	.max_entries = 64,
};

/* This map is for keeping track of throughput (used with report_tput binary) */
/* struct { */
/* 	__uint(type, BPF_MAP_TYPE_ARRAY); */
/* 	__type(key, unsigned int); */
/* 	__type(value, long); */
/* 	__uint(max_entries, 3); */
/* 	__uint(map_flags, BPF_F_MMAPABLE); */
/* } tput SEC(".maps"); */

SEC("prog")
int _prog(struct xdp_md *ctx)
{
#ifdef COPY_STATE
	/* Allocate space */
	ret = bpf_xdp_adjust_tail(ctx, COPY_STATE);
	if (ret)
		return XDP_DROP;

	void *data = (void *)(long)ctx->data;
	void *data_end = (void *)(long)ctx->data_end;

	/* Copy state to the packet and send to userspace */
	unsigned short tmpoff = (long)ctx->data_end - (long)ctx->data;
	tmpoff -= COPY_STATE;
	if (tmpoff > 1500)
		return XDP_DROP;
	struct on_packet_state *pkt_state = data + tmpoff;
	if (pkt_state + 1 > data_end)
		return XDP_DROP;
	if (data + sizeof(*pkt_state) > data_end)
		return XDP_DROP;
	memcpy(pkt_state->state, data, sizeof(*pkt_state));
#endif
	return bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS);
	/* int ret; */
	/* ret = bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS); */
	/* if (ret != XDP_REDIRECT) { */
	/* 	int key = 1; */
	/* 	long *c = bpf_map_lookup_elem(&tput, &key); */
	/* 	if (c) */
	/* 		*c += 1; */
	/* } else { */
	/* 	int key = 2; */
	/* 	long *c = bpf_map_lookup_elem(&tput, &key); */
	/* 	if (c) */
	/* 		*c = (*c + 1); */
	/* } */
	/* return ret; */
}

