/**
 * This function has the function that interface with AF_XDP. Receiving packets
 * and pumping them to the eBPF engine (Brain)
 */
#include <assert.h>
#include <arpa/inet.h>
#include <stdlib.h> // exit
#include <sys/socket.h> // sendto
#include <errno.h>
#include "config.h"
#include "heart.h"
#include "brain.h"
#include "log.h"
#include "include/packet_context.h"
#include "interpose_link.h"
#include "udp_socket.h"

#include <time.h>

/* #define USE_POLL */
#define SHOW_THROUGHPUT
/* #define VM_CALL_BATCHING */
/* #define REPORT_UBPF_OVERHEAD */
#define DBG_CHECK_INCOMING_PKTS


#ifdef USE_POLL
#pragma message "POLL is enabled"
#include <poll.h>
#endif


#ifdef REPORT_UBPF_OVERHEAD
#include "duration_hist.h"
#include <x86intrin.h>
static inline
uint64_t readTSC(void) {
	// _mm_lfence();  // optionally wait for earlier insns to retire before reading the clock
	uint64_t tsc = __rdtsc();
	// _mm_lfence();  // optionally block later instructions until rdtsc retires
	return tsc;
}
#endif

#define ABS(x) (((x) < 0) ? -(x) : (x))

#include "xsk_actions.h"

static uint8_t has_yield = 0;
static uint8_t yield_state[128];
static uint8_t fn_counter = 0;

static inline __attribute__((always_inline)) void
apply_action(struct xsk_socket_info *xsk, const struct xdp_desc *desc,
		verdict_t action)
{
	int ret;
	if (action == SEND) {
		/* tx(xsk, desc, 1); */
		drop(xsk, desc, 1);
	} else if (action == PASS) {
		assert(0);
		uint64_t addr = desc->addr;
		addr = xsk_umem__add_offset_to_addr(addr);
		void *ctx = xsk_umem__get_data(xsk->umem->buffer, addr);

		if (config.use_packet_injection) {
			/* Send to application using the interpose layer */
			/* Note: On the other side interpose.c cxpects both IP and UDP
			 * headers to answer "recvfrom" request.
			 * uBPF program should trim ethernet header.
			 * */
			ret = send_interpose_msg(ctx, desc->len);
			if (ret < 0) {
				// failed to pass packet
				ERROR("Failed to pass packect\n");
			}
			/* Packet data are copied to the shared channel the
			 * AF_XDP descriptor can now be released.
			 * */
		} else {
			/* Send through UDP socket expects IP and UDP headers.
			 * An eBPF/TC program would remove the header added by
			 * socket. As a result receiving application response
			 * to the original sender.
			 * */
			// Use UDP socket to send packet to application
			ret = send_udp_socket_msg(ctx, desc->len);
			if (ret < 0) {
				// failed to pass packet
				ERROR("Failed to pass packet (UDP socket)\n");
			}
		}
		drop(xsk, desc, 1);
	} else if (action == DROP) {
		drop(xsk, desc, 1);
	} else {
		DEBUG("Unknown Action (%d)\n", action);
		drop(xsk, desc, 1);
	}
}

/* void */
/* apply_mix_action(struct xsk_socket_info *xsk, struct xdp_desc **batch, */
/* 		struct pktctxbatch *ctx_batch, uint32_t cnt) */
/* { */
/* 	/1* DEBUG("apply_mix_action\n"); *1/ */
/* 	int ret; */
/* 	// 0 drop, 1 tx, 2 pass 3 yield */
/* 	int action_count[2] = {}; */
/* 	uint32_t index_target[2] = {}; */
/* 	int reserved[2] = {}; */
/* 	struct xsk_ring_prod *rings[2] = {}; */
/* 	rings[0] = &xsk->umem->fq; */
/* 	rings[1] = &xsk->tx; */

/* 	// Count each action and prepare the descriptors */
/* 	for (int i = 0; i < cnt; i++) { */
/* 		batch[i]->len = ctx_batch->pkts[i].pkt_len; */
/* 		batch[i]->addr += ctx_batch->pkts[i].trim_head; */
/* 		if (ctx_batch->rets[i] == DROP) { */
/* 			action_count[0]++; */
/* 		} else if (ctx_batch->rets[i] == SEND) { */
/* 			action_count[1]++; */
/* 		} else if (ctx_batch->rets[i] == YIELD) { */
/* 			yield_state[i] = fn_counter + 1; */
/* 			has_yield = 1; */
/* 		} else { */
/* 			// not implemented yet */
/* 			ERROR("action not found\n"); */
/* 			continue; */
/* 		} */
/* 	} */
/* 	/1* DEBUG("action count 0: %d 1: %d\n", action_count[0], action_count[1]); *1/ */

/* 	// Try to reserve space on rings */
/* 	for (int i = 0; i < 2; i++) { */
/* 		if (action_count[i] < 1) { */
/* 			continue; */
/* 		} */
/* 		ret = xsk_ring_prod__reserve(rings[i], action_count[i], &index_target[i]); */
/* 		if (ret != action_count[i]) { */
/* 			if (ret < 0) { */
/* 				ERROR("Failed to reserve packets on queue!\n"); */
/* 				exit(EXIT_FAILURE); */
/* 			} */
/* 			ERROR("Failed to reserve space on queue\n"); */
/* 			continue; */
/* 		} */
/* 		reserved[i] = 1; */
/* 		/1* DEBUG("reserve action %d: %d\n", i, action_count[i]); *1/ */
/* 	} */

/* 	// Place descriptors on rings */
/* 	for (int i = 0; i < cnt; i++) { */
/* 		uint64_t orig = xsk_umem__extract_addr(batch[i]->addr); */
/* 		if (ctx_batch->rets[i] == DROP) { */
/* 			*xsk_ring_prod__fill_addr(rings[0], index_target[0]) = orig; */
/* 			index_target[0]++; */
/* 		} else if (ctx_batch->rets[i] == SEND) { */
/* 			// Index of descriptors in the umem */
/* 			xsk_ring_prod__tx_desc(rings[1], index_target[1])->addr = orig; */
/* 			xsk_ring_prod__tx_desc(rings[1], index_target[1])->len = batch[i]->len; */
/* 			index_target[1]++; */
/* 		} else { */
/* 			// Not implemented */
/* 			/1* ERROR("placing descriptor action not found\n"); *1/ */
/* 			continue; */
/* 		} */
/* 	} */

/* 	// Submit queue */
/* 	for (int i = 0; i < 2; i++) { */
/* 		if (reserved[i] > 0) { */
/* 			xsk_ring_prod__submit(rings[i], action_count[i]); */
/* 			xsk_ring_cons__release(&xsk->rx, action_count[i]); */
/* 		} */
/* 	} */
/* 	if (action_count[1]) { */
/* 		xsk->outstanding_tx += action_count[1]; */
/* 	} */
/* } */

#ifdef DBG_CHECK_INCOMING_PKTS
#include <linux/if_ether.h>
#include <linux/ip.h>
static int check_is_for_this_server(void *ctx)
{
	static const uint8_t mac[6] = {0x9c,0xdc,0x71,0x5e,0x0f,0x41};
	static const uint32_t server_ip = 0x0101a8c0; // 0xC0A80101
	static char tmp_mac[32];
	struct ethhdr *eth = ctx;
	struct iphdr *ip = (struct iphdr *)(eth + 1);
	if (eth->h_proto != htons(ETH_P_IP) || ip->protocol != IPPROTO_UDP) {
		DEBUG("unexpected inner protocol! (eth: %d ip: %d)\n", eth->h_proto, ip->protocol);
		return -1;
	}
	if (memcmp(eth->h_dest, mac, 6) != 0) {
		snprintf(tmp_mac, 31, "%x:%x:%x:%x:%x:%x",
				eth->h_dest[0],
				eth->h_dest[1],
				eth->h_dest[2],
				eth->h_dest[3],
				eth->h_dest[4],
				eth->h_dest[5]
				);
		DEBUG("wrong mac address: %s\n", tmp_mac);
		DEBUG("\tip: %d.%d.%d.%d\n",
				(ip->daddr) & 0xff,
				(ip->daddr >> 8) & 0xff,
				(ip->daddr >> 16) & 0xff,
				(ip->daddr >> 24) & 0xff
				);
		return -1;
	}
	if (ip->daddr != server_ip) {
		DEBUG("wrong ip %x != %x\n", ip->daddr, server_ip);
		return -1;
	}
	return 0;
}
#endif

/**
 * Poll rx queue of the socket and send received packets to eBPF engine
 *
 * This function has an infinte loop that stops when terminate flag is raised.
 *
 * @param xsk Socket to be polled.
 */
void
pump_packets(struct xsk_socket_info *xsk, struct ubpf_vm *vm)
{
#ifdef SHOW_THROUGHPUT
	static uint64_t pkt_count = 0;
	static uint64_t sent_count = 0;
	struct timespec spec = {};
	clock_gettime(CLOCK_MONOTONIC_COARSE, &spec);
	uint64_t rprt_ts = spec.tv_sec * 1000000 + spec.tv_nsec / 1000;
#endif

	const uint32_t cnt = config.batch_size;
	if (ubpf_set_batch_size(cnt) != 0) {
		ERROR("in uBPF the MAX_BATCH_SZ is set to 128 (for holding the state when yielding)\n");
		return;
	}
	struct xdp_desc batch[128];
	struct pktctx _pkt_ctx_arr[128];
	verdict_t _ret_arr[128];
	memset(batch, 0, sizeof(batch));
	memset(_pkt_ctx_arr, 0, sizeof(_pkt_ctx_arr));
	memset(_ret_arr, 0, sizeof(_ret_arr));

	struct pktctxbatch pkt_batch;
	pkt_batch.cnt  = 0;
	pkt_batch.pkts = _pkt_ctx_arr;
	pkt_batch.rets = _ret_arr;

#ifdef USE_POLL
	struct pollfd fds[1] = {};
	uint32_t empty_rx = 0;
#endif

	if (!config.jitted) {
		ERROR("Configureation disabled JIT.\nIntentionally use JITted mode (ignore the flag)\n");
	}
	char *errmsg;
	ubpf_jit_fn bpf_progs[MAX_NUM_PROGS];
	for (int i = 0; i < config.yield_sz; i++) {
		bpf_progs[i] = ubpf_compile(vm, i, &errmsg);
		if (bpf_progs[i] == NULL) {
			ERROR("Failed to compile: %s\n", errmsg);
			free(errmsg);
			return;
		}
	}

	void *meta[cnt];
	for (int i = 0; i < cnt; i++) {
		meta[i] = malloc(METADATA_SIZE);
		if (meta[i] == NULL) {
			ERROR("Out of memory while allocating metadata region\n");
			for (int j = 0; j < i; j++)
				free(meta[j]);
			return;
		}
		pkt_batch.pkts[i].meta = meta[i];
	}

	for(;;) {
		if (config.terminate)
			break;

		if (xsk->outstanding_tx > 0)
			complete_tx(xsk);
		/* if (ret > 0) */
		/* 	empty_rx--; */

#ifdef USE_POLL
		if (empty_rx >= 8) {
			fds[0].fd = xsk_socket__fd(xsk->xsk);
			fds[0].events = POLLIN; // POLLOUT |
			if (poll(fds, 1, 1000) <= 0)
				continue;
			else
				empty_rx = 0;
		}
#endif

		const uint32_t rx = poll_rx_queue(xsk, batch, cnt);
		if (!rx) {
#ifdef USE_POLL
			empty_rx++;
#endif
			continue;
		}

		/* Received a new batch of packets. reset the state */
		memset(yield_state, 0, rx * sizeof(yield_state[0]));
		fn_counter = 0;

		/* Perpare batch */
		pkt_batch.cnt = rx;

		// Initialize a batch of structures that we pass to eBPF env
		for (uint32_t i = 0; i < rx; i++) {
			const uint64_t addr = batch[i].addr;
			const size_t ctx_len = batch[i].len;
			/* DEBUG("addr: %ld (%ld)- %ld\n", addr, xsk_umem__add_offset_to_addr(addr), ctx_len); */
			void *const ctx = xsk_umem__get_data(xsk->umem->buffer, addr);
			pkt_batch.pkts[i].data = ctx;
			pkt_batch.pkts[i].data_end = ctx + ctx_len;
			pkt_batch.pkts[i].pkt_len = ctx_len;
			pkt_batch.pkts[i].trim_head = 0;
			pkt_batch.rets[i] = YIELD;
			__builtin_prefetch(ctx);
#ifdef DBG_CHECK_INCOMING_PKTS
			if (check_is_for_this_server(ctx) != 0) {
				pkt_batch.rets[i] = DROP;
				/* terminated before the first stage */
				yield_state[i] = (uint8_t)-1;
			}
#endif
		}

		/* Start of the yield-chain */
		do {
			has_yield = 0;

#ifdef VM_CALL_BATCHING
			// TODO: what to do if some packets do not require the next stage?
			// TODO: how to set the ubpf batch offset in the program ?
			/* Pass batch to the vm */
			/* uint64_t start_ts = readTSC(); */
			bpf_progs[fn_counter](&pkt_batch, sizeof(pkt_batch));
			/* uint64_t end_ts = readTSC(); */
			/* calc_latency_from_ts(start_ts, end_ts); */
			apply_mix_action(xsk, batch, &pkt_batch, rx);
#ifdef SHOW_THROUGHPUT
			for (uint32_t i = 0; i < rx; i++)
				if (pkt_batch.rets[i] == SEND)
					sent_count++;
#endif
#else
			ubpf_jit_fn fn = bpf_progs[fn_counter];
			__builtin_prefetch(&ubpf_set_batch_offset, 0, 3);
			for (uint32_t i = 0; i < rx; i++) {
				if (fn_counter != yield_state[i]) {
					/* The verdict is known */
					/* DEBUG("old stage (%d != %d)\n", fn_counter, yield_state[i]); */
					continue;
				}
				struct pktctx *const pktctx = &pkt_batch.pkts[i];

#ifdef REPORT_UBPF_OVERHEAD
				uint64_t start_ts = readTSC();
#endif

				ubpf_set_batch_offset(i);
				pkt_batch.rets[i] = fn(pktctx, sizeof(*pktctx));

#ifdef REPORT_UBPF_OVERHEAD
				uint64_t end_ts = readTSC();
				calc_latency_from_ts(start_ts, end_ts);
#endif
				if (pktctx->pkt_len > config.frame_size) {
					ERROR("Too large packet len!\n");
					pkt_batch.rets[i] = DROP;
				} else {
					batch[i].len = pktctx->pkt_len;
				}
				if (ABS(pktctx->trim_head) > config.headroom) {
					ERROR("trim_head is larger than the configured headroom size\n");
					pkt_batch.rets[i] = DROP;
				} else  {
					if (pktctx->trim_head != 0) {
						DEBUG("trim head: %d\n", pktctx->trim_head);
					}
					/* batch[i]->addr += pktctx->trim_head; */
				}

				/* Update the states if the VM has yielded */
				if (pkt_batch.rets[i] == YIELD) {
					/* move to the new stage */
					yield_state[i] = fn_counter + 1;
					has_yield = 1;
				}
			}
			fn_counter++;
		} while(has_yield && fn_counter < config.yield_sz);
#endif
		if (has_yield) {
			ERROR("Found yield in the last stage!\n");
			has_yield = 0;
		}

		/* NOTE: the actions should applied in order (we are releasing
		 * the rx ring so order matters)
		 * */
		for (uint32_t i = 0; i < rx; i++) {
			verdict_t ret = pkt_batch.rets[i];
			if (ret == YIELD) {
				/* we should not yield in the last round */
				ret = DROP;
			}
#ifdef SHOW_THROUGHPUT
			else if (ret == SEND) {
				sent_count++;
			}
#endif

			/* if (ret == DROP) { */
			/* 	DEBUG("a drop verdict!\n"); */
			/* } */
			apply_action(xsk, &batch[i], ret);
		}
		release_rx_queue(xsk, rx);

#ifdef SHOW_THROUGHPUT
		pkt_count += rx;
		clock_gettime(CLOCK_MONOTONIC_COARSE, &spec);
		uint64_t now = spec.tv_sec * 1000000 + spec.tv_nsec / 1000;
		uint64_t delta = now - rprt_ts;
		if (delta > 2000000) {
			INFO("TP: %d send: %d\n", pkt_count * 1000000 /
					delta, sent_count * 1000000 /
					delta);
			pkt_count = 0;
			sent_count = 0;
			rprt_ts = now;
		}
#endif
	}

	for (int i = 0; i < cnt; i++) {
		free(meta[i]);
	}
#ifdef REPORT_UBPF_OVERHEAD
	print_latency_result();
#endif
}
