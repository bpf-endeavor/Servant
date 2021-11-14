/* #include <string.h> */
/* #include <arpa/inet.h> */
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/in.h>
/* #include <linux/bpf.h> */
/* #include <bpf/bpf_helpers.h> */
/* #include <bpf/bpf_endian.h> */

#include "../../src/include/servant_engine.h"

// from bpf.h
struct bpf_spin_lock {
	uint32_t val;
};

#include "bmc_common.h"


static int bmc_write_reply_main(struct pktctx *ctx);
static int bmc_prepare_packet_main(struct pktctx *ctx);
static uint16_t compute_ip_checksum(struct iphdr *ip);

uint32_t bpf_prog(struct pktctx *arg)
{
	/* return DROP; */
	return bmc_prepare_packet_main(arg);
}

int bmc_write_reply_main(struct pktctx *ctx)
{
	void *data_end = ctx->data_end;
	void *data = ctx->data;
	data = data + (sizeof(struct ethhdr) + sizeof(struct iphdr)
		+ sizeof(struct udphdr) + sizeof(struct memcached_udp_header));
	char *payload = (char *) data;
	unsigned int zero = 0;
	struct parsing_context *pctx = NULL;
	struct memcached_key *key = NULL;
	struct bmc_cache_entry *entry = NULL;
	char mpctx[] = "map_parsing_con";
	char mkeys[] = "map_keys";
	char mkcache[] = "map_kcache";
	pctx = lookup((char *)mpctx, &zero);
	/* DUMP("pctx: %p\n", pctx); */

	while (1) {
		// Loop for multiple keys
		if (key) {
			free_elem(key);
		}
		DUMP("current key: %d\n", pctx->current_key);
		key = lookup((char *)mkeys, &pctx->current_key);
		DUMP("key: %p\n", key);

		unsigned int cache_hit = 1, written = 0;
		unsigned int cache_idx = key->hash % BMC_CACHE_ENTRY_COUNT;
		if (entry) {
			free_elem(entry);
		}
		entry = lookup((char *)mkcache, &cache_idx);

		if (entry->valid && key->hash == entry->hash) {
			// if saved key still matches its corresponding cache entry
			for (int i = 0; i < BMC_MAX_KEY_LENGTH && i < key->len; i++) {
				// compare the saved key with the one stored in the cache entry
				if (key->data[i] != entry->data[6+i]) {
					cache_hit = 0;
				}
			}
			if (cache_hit) {
				// if cache HIT then copy cached data
				unsigned int off;
				for (off = 0; off + sizeof(unsigned long long) < BMC_MAX_CACHE_DATA_SIZE &&
					off + sizeof(unsigned long long) <= entry->len && payload+off+sizeof(unsigned long long) <= data_end; off++) {
					*((unsigned long long *) &payload[off]) = *((unsigned long long *) &entry->data[off]);
					off += sizeof(unsigned long long)-1;
					written += sizeof(unsigned long long);
				}
#pragma clang loop unroll(disable)
				for (; off < BMC_MAX_CACHE_DATA_SIZE && off < entry->len && payload+off+1 <= data_end; off++) {
					payload[off] = entry->data[off];
					written += 1;
				}
			}
		}
		/* bpf_spin_unlock(&entry->lock); */

		// TODO: I am not updating the stats here
		/* struct bmc_stats stats; */
		/* lookup("map_stats", &zero, &stats); */
		/* if (cache_hit) { */
		/*	stats->hit_count++; */
		/* } else { */
		/*	stats->miss_count++; */
		/* } */

		pctx->current_key++;
		if (pctx->current_key == pctx->key_count
				&& (pctx->write_pkt_offset > 0 || written > 0)) {
			// if all saved keys have been processed and a least one cache HIT
			payload[written++] = 'E';
			payload[written++] = 'N';
			payload[written++] = 'D';
			payload[written++] = '\r';
			payload[written++] = '\n';

			/* if (bpf_xdp_adjust_head(ctx, 0 - (int) (sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) */
			/*				+ sizeof(struct memcached_udp_header) + pctx->write_pkt_offset))) { // pop headers + previously written data */
			/*	return XDP_DROP; */
			/* } */

			/* void *data_end = ctx->data_end; */
			/* void *data = ctx->data; */
			struct iphdr *ip = data + sizeof(struct ethhdr);
			struct udphdr *udp = data + sizeof(struct ethhdr) + sizeof(*ip);
			payload = data + sizeof(struct ethhdr) + sizeof(*ip)
				+ sizeof(*udp) + sizeof(struct memcached_udp_header);

			/* if (udp + 1 > data_end) */
			/*	return XDP_PASS; */

			ip->tot_len = ubpf_htons((payload + pctx->write_pkt_offset + written) - (char*)ip);
			ip->check = compute_ip_checksum(ip);
			udp->check = 0; // computing udp checksum is not required
			udp->len = ubpf_htons((payload + pctx->write_pkt_offset + written) - (char*)udp);

			/* bpf_xdp_adjust_tail(ctx, 0 - (int) ((long) data_end - (long) (payload+pctx->write_pkt_offset+written))); // try to strip additional bytes */

			free_elem(pctx);
			free_elem(entry);
			free_elem(key);
			return SEND;
		} else if (pctx->current_key == pctx->key_count) {
			// else if all saved keys have been processed but got no cache HIT;
			// either because of a hash colision or a race with a cache update
			/* stats->hit_misprediction += pctx->key_count; */
			/* bpf_xdp_adjust_head(ctx, ADJUST_HEAD_LEN - (int) (sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct memcached_udp_header))); // pop to the old headers and transmit to netstack */
			// TODO: This is not good :<
			free_elem(pctx);
			free_elem(entry);
			free_elem(key);
			return DROP;
		} else if (pctx->current_key < BMC_MAX_KEY_IN_PACKET) {
			// else if there are still keys to process
			pctx->write_pkt_offset += written; // save packet write offset
			/* if (bpf_xdp_adjust_head(ctx, written)) // push written data */
			/*	return XDP_DROP; */
			/* bpf_tail_call(ctx, &map_progs_xdp, BMC_PROG_XDP_WRITE_REPLY); */
		}
	}
}

static int bmc_prepare_packet_main(struct pktctx *ctx)
{
	// pop empty packet buffer memory to increase the available packet size
	/* if (bpf_xdp_adjust_head(ctx, -ADJUST_HEAD_LEN)) */ 
	/*	return XDP_PASS; */

	void *data = ctx->data;
	struct ethhdr *eth = data;
	struct iphdr *ip = data + sizeof(*eth);
	struct udphdr *udp = data + sizeof(*eth) + sizeof(*ip);
	/* struct memcached_udp_header *memcached_udp_hdr = data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp); */
	/* char *payload = (char *) (memcached_udp_hdr + 1); */
	/* void *old_data = data + ADJUST_HEAD_LEN; */
	/* char *old_payload = (char *) (old_data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp) + sizeof(*memcached_udp_hdr)); */

	// use old headers as a base; then update addresses and ports to create the new headers
	/* memmove(eth, old_data, sizeof(*eth) + sizeof(*ip) + sizeof(*udp) + sizeof(*memcached_udp_hdr)); */

	unsigned char tmp_mac[ETH_ALEN];
	__be32 tmp_ip;
	__be16 tmp_port;

	memcpy(tmp_mac, eth->h_source, ETH_ALEN);
	memcpy(eth->h_source, eth->h_dest, ETH_ALEN);
	memcpy(eth->h_dest, tmp_mac, ETH_ALEN);

	tmp_ip = ip->saddr;
	ip->saddr = ip->daddr;
	ip->daddr = tmp_ip;

	tmp_port = udp->source;
	udp->source = udp->dest;
	udp->dest = tmp_port;

	// push new headers
	/* if (bpf_xdp_adjust_head(ctx, sizeof(*eth) + sizeof(*ip) + sizeof(*udp) */
	/* 			+ sizeof(*memcached_udp_hdr))) */ 
	/*	return XDP_PASS; */

	/* bpf_tail_call(ctx, &map_progs_xdp, BMC_PROG_XDP_WRITE_REPLY); */
	return bmc_write_reply_main(ctx);
}

static uint16_t compute_ip_checksum(struct iphdr *ip)
{
	uint32_t csum = 0;
	uint16_t *next_ip_uint16_t = (uint16_t *)ip;

	ip->check = 0;

#pragma clang loop unroll(full)
	for (int i = 0; i < (sizeof(*ip) >> 1); i++) {
		csum += *next_ip_uint16_t++;
	}

	return ~((csum & 0xffff) + (csum >> 16));
}

