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

/* ADD: Header file for usign Servant featurees */
#include "../../src/include/servant_engine.h"

/* Defining some kernel integer types */
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

#include <linux/in.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
/* #include "bpf_helpers.h" */

#include "bmc_common.h"

#define ADJUST_HEAD_LEN 128

#ifndef memmove
# define memmove(dest, src, n)  __builtin_memmove((dest), (src), (n))
#endif

// Pre define functions
static inline u16 compute_ip_checksum(struct iphdr *ip);

/* bmc_rx_filter_main */
int bpf_prog(struct pktctx *ctx)
{
	void *data_end = ctx->data_end;
	void *data = ctx->data;
	struct ethhdr *eth = data;
	struct iphdr *ip = data + sizeof(*eth);
	void *transp = data + sizeof(*eth) + sizeof(*ip);
	struct udphdr *udp;
	struct tcphdr *tcp;
	char *payload;
	__be16 dport;
	unsigned int zero = 0;

	if (ip + 1 > data_end)
		return PASS;

	switch (ip->protocol) {
		case IPPROTO_UDP:
			udp = (struct udphdr *) transp;
			if (udp + 1 > data_end)
				return PASS;
			dport = udp->dest;
			payload = transp + sizeof(*udp) + sizeof(struct memcached_udp_header);
			break;
		case IPPROTO_TCP:
			return PASS;
		default:
			return PASS;
	}

	if (dport == ubpf_htons(DST_PORT) && payload + 4 <= data_end) {
		if ( payload[0] == 'g' && payload[1] == 'e' &&
				payload[2] == 't' && payload[3] == ' ')
		{ // is this a GET request
			unsigned int off;
#pragma clang loop unroll(disable)
			for (off = 4; off < BMC_MAX_PACKET_LENGTH && payload+off+1 <= data_end && payload[off] == ' '; off++) {} // move offset to the start of the first key
			if (off < BMC_MAX_PACKET_LENGTH) {
				payload = (char *) (data + sizeof(struct ethhdr) +
						sizeof(struct iphdr) + sizeof(struct udphdr) +
						sizeof(struct memcached_udp_header));
				if (payload >= data_end)
					return PASS;
				unsigned int done_parsing = 0, key_len = 0;
				off = 0;

				// compute the key hash
				int tmp = 0;
				done_parsing = 1;
				for (off = 0; off < BMC_MAX_KEY_LENGTH+1 && payload+off+1 <= data_end; off++) {
					if (payload[off] == '\r') {
						done_parsing = 1;
						break;
					}
					else if (payload[off] == ' ') {
						break;
					}
					else if (payload[off] != ' ') {
						/* key->hash ^= payload[off]; */
						/* key->hash *= FNV_PRIME_32; */
						key_len++;
						tmp ^= payload[off];
						tmp *= FNV_PRIME_32;
					}
				}

				if (key_len == 0 || key_len > BMC_MAX_KEY_LENGTH) {
					return PASS;
				}

				u32 cache_idx = tmp % BMC_CACHE_ENTRY_COUNT;
				unsigned int i = 0;
				char tmp_key_data[BMC_MAX_KEY_LENGTH];
				for (; i < key_len && payload+i+1 <= data_end; i++) {
					tmp_key_data[i] = payload[i];
				}

				if (done_parsing) {
					struct ethhdr *eth = data;
					struct iphdr *ip = data + sizeof(*eth);
					struct udphdr *udp = data + sizeof(*eth) + sizeof(*ip);
					struct memcached_udp_header *memcached_udp_hdr = data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp);
					payload = (char *) (memcached_udp_hdr + 1);

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

					payload = (char *) (data + sizeof(struct ethhdr) +
							sizeof(struct iphdr) + sizeof(struct udphdr) +
							sizeof(struct memcached_udp_header));
					unsigned int cache_hit = 1, written = 0;
					for (int i = 0; i < BMC_MAX_KEY_LENGTH && i < key_len; i++) { // compare the saved key with the one stored in the cache entry
						if (tmp_key_data[i] != payload[i]) {
							cache_hit = 1;
						}
					}
					if (cache_hit) {
						char tmp[] = "VALUE 000000000000000000000000000001 200\r\n";
						memcpy(payload, tmp, sizeof(tmp));
						off = sizeof(tmp);
						written += off;
						for (; off < 200 + sizeof(tmp);) {
							*((unsigned long long *) &payload[off]) = 0x68686868; // "hhhh"
							off += sizeof(unsigned long long);
							written += sizeof(unsigned long long);
						}
						payload[off++] = '\r';
						payload[off++] = '\n';
						written += 2;
					}
					payload[written++] = 'E';
					payload[written++] = 'N';
					payload[written++] = 'D';
					payload[written++] = '\r';
					payload[written++] = '\n';


					ip = data + sizeof(struct ethhdr);
					udp = data + sizeof(struct ethhdr) + sizeof(*ip);
					payload = data + sizeof(struct ethhdr) + sizeof(*ip) + sizeof(*udp) + sizeof(struct memcached_udp_header);
					char *pkt_end = (payload + written);
					ip->tot_len = ubpf_htons(pkt_end - (char*)ip);
					ip->check = compute_ip_checksum(ip);
					udp->check = 0; // computing udp checksum is not required
					udp->len = ubpf_htons(pkt_end - (char*)udp);

					// adjust packet len
					ctx->pkt_len = ((long)pkt_end - (long) data);
					return SEND;
				} else {
					return PASS;
				}
			}
		}
	}
	return PASS;
}

static inline u16 compute_ip_checksum(struct iphdr *ip)
{
	u32 csum = 0;
	u16 *next_ip_u16 = (u16 *)ip;

	ip->check = 0;

	for (int i = 0; i < (sizeof(*ip) >> 1); i++) {
		csum += *next_ip_u16++;
	}

	return ~((csum & 0xffff) + (csum >> 16));
}

// to test colisions: keys declinate0123456 and macallums0123456 have hash colision
