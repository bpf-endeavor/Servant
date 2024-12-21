#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <servant/servant_engine.h>

#define DROP_IF_NULL(x) if (x == NULL) {DUMP("Must not happen: %d\n", __LINE__); return DROP;}
#define STRIDE 1025
#define NUM_PAGES (512 * 1024 * 1024)
typedef struct { uint32_t x[1]; } __attribute__((packed)) page_t;

enum {
	GLB_INIT_FLAG,
	GLB_MAP_ADDR,
	GLB_OFF,
	GLB_COUNT_VAR,
};

struct ubpf_map_def global = {
	.type = UBPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(int),
	.value_size = sizeof(uint64_t),
	.max_entries = GLB_COUNT_VAR,
	.nb_hash_functions = 0,
};

struct ubpf_map_def array = {
	.type = UBPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(int),
	.value_size = sizeof(page_t),
	.max_entries = NUM_PAGES,
	.nb_hash_functions = 0,
};

int prog(struct pktctx *ctx)
{
	int index;
	uint64_t *tmp;
	page_t *base, *p;
	uint64_t off;

	// Check if we have initialized the memory pointer;
	index = GLB_INIT_FLAG;
	tmp = userspace_lookup(&global, &index);
	DROP_IF_NULL(tmp);
	if (*tmp == 0) {
		// Initialize:
		// Get the address of the begining of the array and store it in
		// global map
		index = 0;
		p = userspace_lookup(&array, &index);
		index = GLB_MAP_ADDR;
		uint64_t *addr = userspace_lookup(&global, &index);
		DROP_IF_NULL(addr)
		*addr = (uint64_t)p;
		*tmp = 1; // set the flag
		DUMP("Initializing...\n");
	}

	index = GLB_MAP_ADDR;
	tmp = userspace_lookup(&global, &index);
	DROP_IF_NULL(tmp);
	base = (page_t *)*tmp;

	index = GLB_OFF;
	tmp = userspace_lookup(&global, &index);
	DROP_IF_NULL(tmp);
	off = *tmp;

	uint64_t sum = 0;
	for (int i = 0; i < 1; i++) {
		p = &base[off];
		sum += p->x[0];
		off += STRIDE;
		off %= NUM_PAGES;
	}
	if (sum == 123) {
		DUMP("Unexpected!\n");
	}
	*tmp = off; // Store the offset

	return DROP;
}
