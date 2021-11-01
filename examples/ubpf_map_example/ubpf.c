#include <stdint.h>

#include "../../src/include/servant_engine.h"

#include "common.h"

int bpf_prog(void *argv)
{
	const int zero = 0;
	char tmap[] = "test_map";
	char dmap[] = "data_map";
	int *count = lookup((char *)tmap, &zero);
	int index = (*count) % NUM_ENTRIES;
	struct data *d = lookup((char *)dmap, &index);
	DUMP("ETH PROTO: 0x%x\n", d->ether_type);
	DUMP("IP PROTO: 0x%x\n", d->ip_proto);
	DUMP("Payload size: %d bytes\n", d->len);
	DUMP("Payload:\n%s\n", d->payload);
	DUMP("\n");
	DUMP("Count: %d/%d\n", d->count, *count);
	free_elem(count);
	free_elem(d);
	return DROP;
}
