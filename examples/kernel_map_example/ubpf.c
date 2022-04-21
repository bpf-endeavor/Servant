#include <stdint.h>

#include "../../src/include/servant_engine.h"
// or if headers are installed use:
//     #include <servant/servant_engine.h>

#include "common.h"

int bpf_prog(struct pktctx *ctx)
{
	const int zero = 0;
	char tmap[] = "test_map";
	char dmap[] = "data_map";
	unsigned int *count = lookup((char *)&tmap, &zero);
	int index = (*count) % NUM_ENTRIES;
	struct data *d = lookup((char *)dmap, &index);
	DUMP("ETH PROTO: 0x%x\n", d->ether_type);
	DUMP("IP PROTO: 0x%x\n", d->ip_proto);
	DUMP("Payload size: %d bytes\n", d->len);
	DUMP("Payload:\n%s\n", d->payload);
	DUMP("Count: %d/%d\n", d->count, *count);
	return DROP;
}
