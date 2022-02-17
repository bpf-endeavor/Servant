#include <servant/servant_engine.h>

/* Add some definitions which should probably be part of ubpf.h */
enum bpf_map_type {
	BPF_MAP_TYPE_ARRAY = 1,
	BPF_MAP_TYPE_BLOOMFILTER = 2,
	BPF_MAP_TYPE_COUNTMIN = 3,
	BPF_MAP_TYPE_HASHMAP = 4,
};

struct bpf_map_def {
	enum bpf_map_type type;
	unsigned int key_size;
	unsigned int value_size;
	unsigned int max_entries;
	unsigned int nb_hash_functions;
};

#define KEY 365
#define VAL 7460

struct bpf_map_def test_map = {
	.type = BPF_MAP_TYPE_HASHMAP,
	.key_size = sizeof(int),
	.value_size = sizeof(long long int),
	.max_entries = 100,
	.nb_hash_functions = 0,
};

static inline __attribute__((always_inline)) void __initialize(void);

int bpf_prog(void *pkt) {
	// TODO: enable runtime to have a way of initializing the maps
	// probably using a RPC framework like Katran.
	/* static int __flag = 0; */
	/* if (!__flag) */
	{
		/* __flag = 1; */
		__initialize();
	}

	DUMP("A packet received\n");
	const int key = KEY;
	int * val = userspace_lookup(&test_map, &key);
	if (val != NULL && *val == VAL) {
		DUMP("Test passed\n");
		return DROP;
	}
	DUMP("Test failed\n");
	return DROP;
}

static inline __attribute__((always_inline)) void __initialize(void)
{
	DUMP("Initializing\n");
	const int key = KEY;
	long long int value = VAL;
	userspace_update(&test_map, &key, &value);
}

