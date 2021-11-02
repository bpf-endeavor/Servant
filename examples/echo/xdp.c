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

SEC("ubpf_dump_packet")
int _ubpf_dump_packet(struct xdp_md *ctx)
{
	return bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS);
}

