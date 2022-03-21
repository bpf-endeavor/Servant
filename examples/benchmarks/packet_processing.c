#include <linux/ipv6.h>
#include "general_header.h"

#define REPEAT 64

#define IPV4_HDR_LEN_NO_OPT 20
#define PCKT_FRAGMENTED 65343
#define FURTHER_PROCESSING -1
#define BE_ETH_P_IP 8
#define BE_ETH_P_IPV6 56710

struct ctl_value {
	union {
		__u64 value;
		__u32 ifindex;
		__u8 mac[6];
	};
};

// flow metadata
struct flow_key {
	union {
		__be32 src;
		__be32 srcv6[4];
	};
	union {
		__be32 dst;
		__be32 dstv6[4];
	};
	union {
		__u32 ports;
		__u16 port16[2];
	};
	__u8 proto;
};

// client's packet metadata
struct packet_description {
	struct flow_key flow;
	__u32 real_index;
	__u8 flags;
	// dscp / ToS value in client's packet
	__u8 tos;
};

// vip's definition for lookup
struct vip_definition {
	union {
		__be32 vip;
		__be32 vipv6[4];
	};
	__u16 port;
	__u8 proto;
};

// result of vip's lookup
struct vip_meta {
  __u32 flags;
  __u32 vip_num;
};

#ifdef ISUBPF
sinline int bpf_prog(CONTEXT *ctx);
/**
 * Entry of the uBPF program
 */
int batch_processing_entry(struct pktctxbatch *batch)
{
	for (int i = 0; i < batch->cnt; i++) {
		batch->rets[i] = bpf_prog(&batch->pkts[i]);
	}
	return 0;
}
#endif

__attribute__((__always_inline__)) static inline int process_l3_headers(
		struct packet_description* pckt,
		__u8* protocol,
		__u64 off,
		__u16* pkt_bytes,
		void* data,
		void* data_end,
		bool is_ipv6) {
	__u64 iph_len;
	int action;
	struct iphdr* iph;
	struct ipv6hdr* ip6h;
	if (is_ipv6) {
		ip6h = data + off;
		if ((void *)(ip6h + 1) > data_end) {
			return XDP_DROP;
		}

		iph_len = sizeof(struct ipv6hdr);
		*protocol = ip6h->nexthdr;
		pckt->flow.proto = *protocol;

		// copy tos from the packet
		pckt->tos = (ip6h->priority << 4) & 0xF0;
		pckt->tos = pckt->tos | ((ip6h->flow_lbl[0] >> 4) & 0x0F);

		*pkt_bytes = bpf_ntohs(ip6h->payload_len);
		off += iph_len;
		if (*protocol == IPPROTO_FRAGMENT) {
			// we drop fragmented packets
			return XDP_DROP;
		} else if (*protocol == IPPROTO_ICMPV6) {
			/* action = parse_icmpv6(data, data_end, off, pckt); */
			action = XDP_DROP;
			if (action >= 0) {
				return action;
			}
		} else {
			memcpy(pckt->flow.srcv6, ip6h->saddr.s6_addr32, 16);
			memcpy(pckt->flow.dstv6, ip6h->daddr.s6_addr32, 16);
		}
	} else {
		iph = data + off;
		if ((void *)(iph + 1) > data_end) {
			return XDP_DROP;
		}
		// ihl contains len of ipv4 header in 32bit words
		if (iph->ihl != 5) {
			// if len of ipv4 hdr is not equal to 20bytes that means that header
			// contains ip options, and we dont support em
			return XDP_DROP;
		}
		pckt->tos = iph->tos;
		*protocol = iph->protocol;
		pckt->flow.proto = *protocol;
		*pkt_bytes = bpf_ntohs(iph->tot_len);
		off += IPV4_HDR_LEN_NO_OPT;

		if (iph->frag_off & PCKT_FRAGMENTED) {
			// we drop fragmented packets.
			return XDP_DROP;
		}
		if (*protocol == IPPROTO_ICMP) {
			/* action = parse_icmp(data, data_end, off, pckt); */
			action = XDP_DROP;
			if (action >= 0) {
				return action;
			}
		} else {
			pckt->flow.src = iph->saddr;
			pckt->flow.dst = iph->daddr;
		}
	}
	iph->tos++;
	return FURTHER_PROCESSING;
}

SEC("prog")
int bpf_prog(CONTEXT *ctx)
{
	unsigned char is_ipv6;
	__u64 off;
	void* data = (void*)(long)ctx->data;
	void* data_end = (void*)(long)ctx->data_end;
	struct ctl_value* cval;
	struct real_definition* dst = NULL;
	struct packet_description pckt = {};
	struct vip_definition vip = {};
	struct vip_meta* vip_info;
	struct lb_stats* data_stats;
	__u64 iph_len;
	__u8 protocol;
	__u16 original_sport;

	struct ethhdr* eth = data;
	__u32 eth_proto;
	off = sizeof(struct ethhdr);

	if (data + off > data_end) {
		// bogus packet, len less than minimum ethernet frame size
		return XDP_DROP;
	}

	eth_proto = eth->h_proto;

	if (eth_proto == BE_ETH_P_IP) {
		is_ipv6 = 0;
	} else if (eth_proto == BE_ETH_P_IPV6) {
		is_ipv6 = 1;
	} else {
		// pass to tcp/ip stack
		return XDP_DROP;
	}

	int action;
	__u32 vip_num;
	__u32 mac_addr_pos = 0;
	__u16 pkt_bytes;
	for (int i = 0; i < REPEAT; i++) {
		action = process_l3_headers(
				&pckt, &protocol, off, &pkt_bytes, data, data_end,
				is_ipv6);
		if (action >= 0) {
			return action;
		}

		/* add something so that compiler keeps the calculation */
		if (pkt_bytes == 0)
			return XDP_DROP;
		int tmp = pckt.flow.src ^ pckt.flow.dst;
		if (tmp == 0)
			return XDP_DROP;
		if (data + 48 < data_end) {
			*((int *)(data + 23)) += 1;
		}
	}

	/* INC_TPUT; */
	return XDP_DROP;
}
