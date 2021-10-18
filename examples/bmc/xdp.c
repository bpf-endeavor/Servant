/*
 *  Software Name : bmc-cache
 *  SPDX-FileCopyrightText: Copyright (c) 2021 Orange
 *  SPDX-License-Identifier: LGPL-2.1-only
 *
 *  This software is distributed under the
 *  GNU Lesser General Public License v2.1 only.
 *
 *  Author: Yoann GHIGOFF <yoann.ghigoff@orange.com> et al.
 */

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <linux/pkt_cls.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#include "bmc_common.h"
/* #include "debug.h" */

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;


#define ADJUST_HEAD_LEN 128

#ifndef memmove
# define memmove(dest, src, n)  __builtin_memmove((dest), (src), (n))
#endif

#ifndef memcpy
# define memcpy(dest, src, n)   __builtin_memcpy((dest), (src), (n))
#endif

/*
 * eBPF maps
*/

/* cache */
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, u32);
	__type(value, struct bmc_cache_entry);
	__uint(max_entries, BMC_CACHE_ENTRY_COUNT);
} map_kcache SEC(".maps");


struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__type(key, unsigned int);
	__type(value, struct memcached_key);
	__uint(max_entries, BMC_MAX_KEY_IN_PACKET);
} map_keys SEC(".maps");

struct bpf_map_def SEC("maps") map_parsing_context = {
	.type = BPF_MAP_TYPE_PERCPU_ARRAY,
	.key_size = sizeof(unsigned int),
	.value_size = sizeof(struct parsing_context),
	.max_entries = 1,
};

/* stats */
struct bpf_map_def SEC("maps") map_stats = {
	.type = BPF_MAP_TYPE_PERCPU_ARRAY,
	.key_size = sizeof(unsigned int),
	.value_size = sizeof(struct bmc_stats),
	.max_entries = 1,
};

/* program maps */
struct bpf_map_def SEC("maps") map_progs_xdp = {
	.type = BPF_MAP_TYPE_PROG_ARRAY,
	.key_size = sizeof(u32),
	.value_size = sizeof(u32),
	.max_entries = BMC_PROG_XDP_MAX,
};

struct bpf_map_def SEC("maps") map_progs_tc = {
	.type = BPF_MAP_TYPE_PROG_ARRAY,
	.key_size = sizeof(u32),
	.value_size = sizeof(u32),
	.max_entries = BMC_PROG_TC_MAX,
};

/* NOTE (Farbod): For debugging */
struct bpf_map_def SEC("maps") debug_map = {
	.type = BPF_MAP_TYPE_PERF_EVENT_ARRAY,
	.key_size = sizeof(int),
	.value_size = sizeof(u32),
	.max_entries = 2,
};

// For sending to ubpf
struct bpf_map_def SEC("maps") xsks_map = {
	.type = BPF_MAP_TYPE_XSKMAP,
	.key_size = sizeof(int),
	.value_size = sizeof(int),
	.max_entries = 64,
};

SEC("bmc_rx_filter")
int bmc_rx_filter_main(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ethhdr *eth = data;
	struct iphdr *ip = data + sizeof(*eth);
	void *transp = data + sizeof(*eth) + sizeof(*ip);
	struct udphdr *udp;
	struct tcphdr *tcp;
	char *payload;
	__be16 dport;

	if (ip + 1 > data_end)
		return XDP_PASS;

	switch (ip->protocol) {
		case IPPROTO_UDP:
			udp = (struct udphdr *) transp;
			if (udp + 1 > data_end)
				return XDP_PASS;
			dport = udp->dest;
			payload = transp + 
				sizeof(*udp) + 
				sizeof(struct memcached_udp_header);
			break;
		case IPPROTO_TCP:
			tcp = (struct tcphdr *) transp;
			if (tcp + 1 > data_end)
				return XDP_PASS;
			dport = tcp->dest;
			payload = transp + sizeof(*tcp);
			break;
		default:
			return XDP_PASS;
	}

	if (dport == bpf_htons(DST_PORT) && payload+4 <= data_end) {

		if (ip->protocol == IPPROTO_UDP &&
			payload[0] == 'g' &&
			payload[1] == 'e' &&
			payload[2] == 't' &&
			payload[3] == ' ') 
		{ // is this a GET request
			unsigned int zero = 0;
			struct bmc_stats *stats =
				bpf_map_lookup_elem(&map_stats, &zero);
			if (!stats) {
				return XDP_PASS;
			}
			stats->get_recv_count++;

			struct parsing_context *pctx = 
				bpf_map_lookup_elem(&map_parsing_context, &zero);
			if (!pctx) {
				return XDP_PASS;
			}
			pctx->key_count = 0;
			pctx->current_key = 0;
			pctx->write_pkt_offset = 0;

			unsigned int off;
			// move offset to the start of the first key
#pragma clang loop unroll(disable)
			for (off = 4; 
				off < BMC_MAX_PACKET_LENGTH && 
				payload+off+1 <= data_end &&
				payload[off] == ' '; off++) {}
			if (off < BMC_MAX_PACKET_LENGTH) {
				pctx->read_pkt_offset = off; // save offset
				if (bpf_xdp_adjust_head(ctx,
					(int)(sizeof(*eth) + sizeof(*ip) +
						sizeof(*udp) +
						sizeof(struct memcached_udp_header) + off)))
				{ // push headers + 'get ' keyword
					return XDP_PASS;
				}
				bpf_tail_call(ctx, &map_progs_xdp, BMC_PROG_XDP_HASH_KEYS);
			}
		}
		else if (ip->protocol == IPPROTO_TCP) {
			bpf_tail_call(ctx, &map_progs_xdp, BMC_PROG_XDP_INVALIDATE_CACHE);
		}
	}

	return XDP_PASS;
}


SEC("bmc_hash_keys")
int bmc_hash_keys_main(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	char *payload = (char *) data;
	unsigned int zero = 0;

	if (payload >= data_end)
		return XDP_PASS;

	struct parsing_context *pctx = bpf_map_lookup_elem(&map_parsing_context, &zero);
	if (!pctx) {
		return XDP_PASS;
	}

	struct memcached_key *key = bpf_map_lookup_elem(&map_keys, &pctx->key_count);
	if (!key) {
		return XDP_PASS;
	}
	key->hash = FNV_OFFSET_BASIS_32;

	unsigned int off, done_parsing = 0, key_len = 0;

	// compute the key hash
#pragma clang loop unroll(disable)
	for (off = 0; off < BMC_MAX_KEY_LENGTH+1 && payload+off+1 <= data_end; off++) {
		if (payload[off] == '\r') {
			done_parsing = 1;
			break;
		}
		else if (payload[off] == ' ') {
			break;
		}
		else if (payload[off] != ' ') {
			key->hash ^= payload[off];
			key->hash *= FNV_PRIME_32;
			key_len++;
		}
	}

	if (key_len == 0 || key_len > BMC_MAX_KEY_LENGTH) {
		bpf_xdp_adjust_head(ctx, 0 - (sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct memcached_udp_header) + pctx->read_pkt_offset)); // unexpected key, let the netstack handle it
		return XDP_PASS;
	}

	u32 cache_idx = key->hash % BMC_CACHE_ENTRY_COUNT;
	struct bmc_cache_entry *entry = bpf_map_lookup_elem(&map_kcache, &cache_idx);
	if (!entry) { // should never happen since cache map is of type BPF_MAP_TYPE_ARRAY
		return XDP_PASS;
	}

	bpf_spin_lock(&entry->lock);
	if (entry->valid && entry->hash == key->hash) { // potential cache hit
		bpf_spin_unlock(&entry->lock);
		unsigned int i = 0;
#pragma clang loop unroll(disable)
		for (; i < key_len && payload+i+1 <= data_end; i++) { // copy the request key to compare it with the one stored in the cache later
			key->data[i] = payload[i];
		}
		key->len = key_len;
		pctx->key_count++;
	} else { // cache miss
		bpf_spin_unlock(&entry->lock);
		struct bmc_stats *stats = bpf_map_lookup_elem(&map_stats, &zero);
		if (!stats) {
			return XDP_PASS;
		}
		stats->miss_count++;
	}

	if (done_parsing) { // the end of the request has been reached
		bpf_xdp_adjust_head(ctx, 0 - (sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct memcached_udp_header) + pctx->read_pkt_offset)); // pop headers + 'get ' + previous keys
		if (pctx->key_count > 0) {
			/* bpf_tail_call(ctx, &map_progs_xdp, BMC_PROG_XDP_PREPARE_PACKET); */
			return bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS);
		}
	} else { // more keys to process
		off++; // move offset to the start of the next key
		pctx->read_pkt_offset += off;
		if (bpf_xdp_adjust_head(ctx, off)) // push the previous key
			return XDP_PASS;
		bpf_tail_call(ctx, &map_progs_xdp, BMC_PROG_XDP_HASH_KEYS);
	}

	return XDP_PASS;
}

SEC("bmc_invalidate_cache")
int bmc_invalidate_cache_main(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ethhdr *eth = data;
	struct iphdr *ip = data + sizeof(*eth);
	struct tcphdr *tcp = data + sizeof(*eth) + sizeof(*ip);
	char *payload = (char *) (tcp + 1);
	unsigned int zero = 0;

	if (payload >= data_end)
		return XDP_PASS;

	struct bmc_stats *stats = bpf_map_lookup_elem(&map_stats, &zero);
	if (!stats) {
		return XDP_PASS;
	}

	u32 hash;
	int set_found = 0, key_found = 0;

#pragma clang loop unroll(disable)
	for (unsigned int off = 0; off < BMC_MAX_PACKET_LENGTH && payload+off+1 <= data_end; off++) {

		if (set_found == 0 && payload[off] == 's' && payload+off+3 <= data_end && payload[off+1] == 'e' && payload[off+2] == 't') {
			set_found = 1;
			off += 3; // move offset after the set keywork, at the next iteration 'off' will either point to a space or the start of the key
			stats->set_recv_count++;
		}
		else if (key_found == 0 && set_found == 1 && payload[off] != ' ') {
			if (payload[off] == '\r') { // end of packet
				set_found = 0;
				key_found = 0;
			} else { // found the start of the key
				hash = FNV_OFFSET_BASIS_32;
				hash ^= payload[off];
				hash *= FNV_PRIME_32;
				key_found = 1;
			}
		}
		else if (key_found == 1) {
			if (payload[off] == ' ') { // found the end of the key
				u32 cache_idx = hash % BMC_CACHE_ENTRY_COUNT;
				struct bmc_cache_entry *entry = bpf_map_lookup_elem(&map_kcache, &cache_idx);
				if (!entry) {
					return XDP_PASS;
				}
				bpf_spin_lock(&entry->lock);
				if (entry->valid) {
					entry->valid = 0;
					stats->invalidation_count++;
				}
				bpf_spin_unlock(&entry->lock);
				set_found = 0;
				key_found = 0;
			}
			else { // still processing the key
				hash ^= payload[off];
				hash *= FNV_PRIME_32;
			}
		}
	}

	return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
// to test colisions: keys declinate0123456 and macallums0123456 have hash colision
