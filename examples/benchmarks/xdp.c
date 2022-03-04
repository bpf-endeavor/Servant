/* This is XDP program used for testing AF_XDP benchmarks */
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>


#define FAIL_PTR_BOUND(s, end) (s + 1) > (void *)end

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


SEC("prog")
int _prog(struct xdp_md *ctx)
{
	return bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS);
}

