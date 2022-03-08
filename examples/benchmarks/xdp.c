/* This is XDP program used for testing AF_XDP benchmarks */
#include <string.h>
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>


#define FAIL_PTR_BOUND(s, end) (s + 1) > (void *)end

#ifdef COPY_STATE
#define STATE_ON_PACKET_SIZE 256
struct on_packet_state {
	char state[COPY_STATE];
} __attribute__((packed));
#endif

struct bpf_map_def SEC("maps") xsks_map = {
	.type = BPF_MAP_TYPE_XSKMAP,
	.key_size = sizeof(int),
	.value_size = sizeof(int),
	.max_entries = 64,
};

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, unsigned int);
	__type(value, long);
	__uint(max_entries, 1);
	__uint(map_flags, BPF_F_MMAPABLE);
} tput SEC(".maps");

static inline __attribute__((__always_inline__)) void
mymemcpy(void *dst, void *src, unsigned short size)
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

SEC("prog")
int _prog(struct xdp_md *ctx)
{
#ifdef COPY_STATE
	/* Allocate space */
	int ret = bpf_xdp_adjust_tail(ctx, COPY_STATE);
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
	/* memcpy(pkt_state->state, data, sizeof(*pkt_state)); */
	mymemcpy(pkt_state->state, data, sizeof(*pkt_state));
#endif
	return bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS);
}

