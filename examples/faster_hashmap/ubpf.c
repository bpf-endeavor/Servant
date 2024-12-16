#include "test_common.h"

int bpf_prog(struct pktctx *pkt) {
	int ret;
	int zero = 0;
	flow_t *flow = pkt->meta;
	val_t *val;

	ret = heavy_processing(pkt);
	if (ret == 1243) {
		ubpf_print("unexpected fibonacci number\n");
	}

	if (parse_flow(pkt, flow) != 0)
		return DROP;
	val = userspace_lookup(&table, flow);
	if (val == NULL) {
		val_t v = {
			.index = 1,
			.timestamp = 1234,
		};
		userspace_update(&table, flow, &v);
		return DROP;
	}
	if (val->index != 1) {
		ubpf_print("unexpected value\n");
	}
	return DROP;
}
