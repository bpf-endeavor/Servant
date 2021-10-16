#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
/* #include <bpf/bpf_endian.h> */

/* #include <linux/if_ether.h> // ethhdr */
/* #include <linux/ip.h> // iphdr */
/* #include <linux/in.h> // IPPROTO_* */
/* #include <linux/udp.h> // udphdr */

#define FAIL_PTR_BOUND(s, end) (s + 1) > (void *)end

struct bpf_map_def SEC("maps") xsks_map = {
	.type = BPF_MAP_TYPE_XSKMAP,
	.key_size = sizeof(int),
	.value_size = sizeof(int),
	.max_entries = 64,
};

struct bpf_map_def SEC("maps") test_map = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(int),
	.value_size = sizeof(int),
	.max_entries = 1,
};


SEC("ubpf_map_test")
int _ubpf_map_test(struct xdp_md *ctx)
{
	const int zero = 0;
	unsigned int *val;
	val = bpf_map_lookup_elem(&test_map, &zero);
	if (val == NULL)
		return XDP_PASS;
	*val = *val + 1; // increment value
	return bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS);
}

