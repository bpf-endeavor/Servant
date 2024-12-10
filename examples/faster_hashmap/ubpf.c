#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include <servant/servant_engine.h>

typedef struct {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t  protocol;
} flow_t;

typedef struct {
	uint32_t index;
	uint64_t timestamp;
} val_t;

struct ubpf_map_def table = {
	.type = UBPF_MAP_TYPE_HASHMAP,
	.key_size = sizeof(flow_t),
	.value_size = sizeof(val_t),
	.max_entries = 8000000,
	.nb_hash_functions = 0,
};

static inline __attribute__((always_inline))
int parse_flow(struct pktctx *pkt, flow_t *flow)
{
	struct ethhdr *eth  = pkt->data;
	struct iphdr *ip = (struct iphdr *)(eth + 1);
	struct udphdr *udp = (struct udphdr *)(ip + 1);
	if ((void *)(udp + 1) > pkt->data_end)
		return -1;
	flow->src_ip = ip->saddr;
	flow->dst_ip = ip->daddr;
	flow->src_port = udp->source;
	flow->dst_port = udp->dest;
	flow->protocol = ip->protocol;
	return 0;
}

static inline __attribute__((always_inline))
int heavy_processing(struct pktctx *pkt)
{
	const int target = 1 << 10;
	int a = 1, b = 1, tmp = 1;
	for (int i = 2; i < target; i++) {
		tmp = a + b;
		a = b;
		b = tmp;
	}
	return b;
}

int bpf_prog(struct pktctx *pkt) {
	int ret;
	flow_t flow;
	val_t *val;

	ret = heavy_processing(pkt);
	if (ret == 1243) {
		ubpf_print("unexpected fibonacci number\n");
	}

	if (parse_flow(pkt, &flow) != 0)
		return DROP;
	val = userspace_lookup(&table, &flow);
	if (val == NULL) {
		val_t v = {
			.index = 1,
			.timestamp = 1234,
		};
		userspace_update(&table, &flow, &v);
		return DROP;
	}
	if (val->index != 1) {
		ubpf_print("unexpected value\n");
	}
	return DROP;
}