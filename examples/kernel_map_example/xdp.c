#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#include <linux/if_ether.h> // ethhdr
#include <linux/ip.h> // iphdr
#include <linux/in.h> // IPPROTO_*
#include <linux/udp.h> // udphdr

#include "common.h"

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

struct bpf_map_def SEC("maps") data_map = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(int),
	.value_size = sizeof(struct data),
	.max_entries = NUM_ENTRIES,
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
	int index = *val % NUM_ENTRIES;
	struct data *d = bpf_map_lookup_elem(&data_map, &index);
	if (d == NULL)
		return XDP_PASS;
	d->count = *val;

	struct ethhdr *eth = (void *)(long)ctx->data;
	struct iphdr *ip = (struct iphdr *)(eth + 1);
	struct udphdr *udp = (struct udphdr *)(ip + 1); //
	char *payload = (char *)(udp + 1);
	void *data_end = (void *)(long)ctx->data_end;

	if (FAIL_PTR_BOUND(eth, data_end))
		return XDP_PASS;

	d->ether_type = bpf_ntohs(eth->h_proto);
	if (d->ether_type == ETH_P_IP) {
		if (FAIL_PTR_BOUND(ip, data_end))
			return XDP_PASS;
		d->ip_proto = ip->protocol;
		if (ip->protocol == IPPROTO_UDP) {
			if (FAIL_PTR_BOUND(udp, data_end)) 
				return XDP_PASS;
			__u32 len = bpf_ntohs(udp->len);
			// Remove header length;
			len = len - sizeof(struct udphdr);
			/* if (len > (MAX_PAYLOAD_SIZE - 1)) */
			/* 	len = MAX_PAYLOAD_SIZE - 1; */
			if (((payload + len) > data_end))
				return XDP_PASS;
			d->len = len;
			for (__u32 i = 0; i < len; i++) {
				if ((payload + i + 1) > data_end)
					return XDP_PASS;
				if (i + 1 > MAX_PAYLOAD_SIZE)
					break;
				// copy data to map
				d->payload[i] = payload[i];
			}
			return bpf_redirect_map(&xsks_map, ctx->rx_queue_index,
					XDP_PASS);
		} else {
			return XDP_PASS;
		}
	} else {
		return XDP_PASS;
	}
}

