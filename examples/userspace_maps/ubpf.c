#include <linux/if_ether.h>
#include <linux/ip.h>

#include <servant/servant_engine.h>

#define KEY 365
#define VAL 7460

struct ubpf_map_def test_map = {
	.type = UBPF_MAP_TYPE_HASHMAP,
	.key_size = sizeof(int),
	.value_size = sizeof(long long int),
	.max_entries = 100,
	.nb_hash_functions = 0,
};

struct ubpf_map_def count_map = {
	.type = UBPF_MAP_TYPE_HASHMAP,
	.key_size = sizeof(uint32_t),
	.value_size = sizeof(long long int),
	.max_entries = 1024,
	.nb_hash_functions = 0,
};

__attribute__((__always_inline__)) static inline
int bpf_prog(struct pktctx *ctx);

/**
 * Entry of the uBPF program
 */
int batch_processing_entry(struct pktctxbatch *batch)
{
	for (int i = 0; i < batch->cnt; i++)
		batch->rets[i] = bpf_prog(&batch->pkts[i]);
	return 0;
}

__attribute__((__always_inline__)) static inline
void swap_mac(struct ethhdr *eth)
{
	unsigned char tmp;
	for (int i = 0; i < 6; i++) {
		tmp = eth->h_source[i];
		eth->h_source[i] = eth->h_dest[i];
		eth->h_dest[i] = tmp;
	}
}

__attribute__((__always_inline__)) static inline
int bpf_prog(struct pktctx *pkt) {
	struct ethhdr *eth  = pkt->data;
	int one = 1;
	if (eth->h_proto == ubpf_ntohs(ETH_P_IP)) {
		struct iphdr *ip = (struct iphdr *)eth + 1;
		uint32_t key = ip->saddr;
		uint32_t *count = userspace_lookup(&count_map, &key);
		if (!count) {
			// add the key to the map
			userspace_update(&count_map, &key, &one);
			return DROP;
		}
		*count = *count + 1;
		swap_mac(eth);
		return SEND;
	}
	/* const int key = KEY; */
	/* int * val = userspace_lookup(&test_map, &key); */
	/* if (val != NULL && *val == VAL) { */
	/* 	DUMP("Test passed\n"); */
	/* 	return DROP; */
	/* } */
	/* DUMP("Test failed\n"); */
	return DROP;
}
