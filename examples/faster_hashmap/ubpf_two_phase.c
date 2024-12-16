#include "test_common.h"

static inline __attribute__((always_inline)) __attribute__((unused))
void DUMP_FLOW(flow_t *flow) {
	DUMP("[*] flow: %d:%d   %d:%d\n",
		flow->src_ip, ubpf_ntohs(flow->src_port),
		flow->dst_ip, ubpf_ntohs(flow->dst_port));
}

int p1(struct pktctx *pkt)
{
	int ret;
	int zero = 0;
	flow_t *flow = pkt->meta;

	ret = heavy_processing(pkt);
	if (ret == 1243) {
		DUMP("unexpected fibonacci number\n");
	}

	if (parse_flow(pkt, flow) != 0) {
		DUMP("Failed to parse flow\n");
		return DROP;
	}

	ret = userspace_lookup_p1(&table, flow);
	if (ret != 0) {
		DUMP("Failed to perform phase 1 of lookup (ret:%d)\n", ret);
		return DROP;
	}
	return YIELD;
}

int p2(struct pktctx *pkt)
{
	int zero = 0;
	flow_t *flow = pkt->meta;

	val_t *val = userspace_lookup_p2(&table, flow);
	if (val == NULL) {
		val_t v = {
			.index = 1,
			.timestamp = 1234,
		};
		/* DUMP("update flow: %d:%d   %d:%d\n", */
		/* 	flow.src_ip, ubpf_ntohs(flow.src_port), */
		/* 	flow.dst_ip, ubpf_ntohs(flow.dst_port)); */
		userspace_update(&table, flow, &v);
		return DROP;
	}

	if (val->index != 1) {
		DUMP("unexpected value (@%p, i: %d)\n", val, val->index);
		/* val = userspace_lookup(&table, &flow); */
		/* if (val == NULL) { */
		/* 	DUMP("very unexpected! the normal lookup failed\n"); */
		/* 	DUMP("flow: %d:%d   %d:%d\n", */
		/* 		flow.src_ip, ubpf_ntohs(flow.src_port), */
		/* 		flow.dst_ip, ubpf_ntohs(flow.dst_port)); */
		/* 	val_t v = { */
		/* 		.index = 1, */
		/* 		.timestamp = 1234, */
		/* 	}; */
		/* 	ret = userspace_update(&table, &flow, &v); */
		/* 	if (ret != 0) { */
		/* 		DUMP("failed to update the map!\n"); */
		/* 	} */
		/* 	return DROP; */
		/* } */
		/* DUMP("actual lookup (@%p, i: %d)\n", val, val->index); */
	}
	/* DUMP("success\n"); */
	return DROP;
}
