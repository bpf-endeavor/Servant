/* #include <linux/bpf.h> */
/* #include <bpf/bpf_helpers.h> */
#include "general_header.h"
#include "map_detail.h"

#define REPEAT 32

#ifdef ISXDP
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, struct keyobj);
	__type(value, struct valobj);
	__uint(max_entries, MAX_ENTRY);
	/* __uint(map_flags, NO_FLAGS); */
} hash_map SEC(".maps");
#else
struct ubpf_map_def hash_map = {
	.type = UBPF_MAP_TYPE_HASHMAP,
	.key_size = sizeof(struct keyobj),
	.value_size = sizeof(struct valobj),
	.max_entries = MAX_ENTRY,
	.nb_hash_functions = 0,
};
#endif

SEC("prog")
int bpf_prog(CONTEXT *ctx)
{
	struct keyobj key = {};
	struct valobj *valptr = NULL;
	for (int i = 0; i < REPEAT; i++) {
		*(int *)(&key.obj) = i;
#ifdef ISXDP
		valptr = bpf_map_lookup_elem(&hash_map, &key);
#else
		valptr = userspace_lookup(&hash_map, &key);
#endif
		if (valptr == NULL) {
			return XDP_DROP;
		}
		*(int *)(&valptr->obj) += 1;
	}
	return XDP_DROP;
}
