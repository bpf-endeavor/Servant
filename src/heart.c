/**
 * This function has the function that interface with AF_XDP. Receiving packets
 * and pumping them to the eBPF engine (Brain)
 */
#include <stdlib.h> // exit
#include <sys/socket.h> // sendto
#include <errno.h>
#include "config.h"
#include "heart.h"
#include "brain.h"
#include "log.h"
#include "include/packet_context.h"
#include "interpose_link.h"

#include <time.h>

/* #define SHOW_THROUGHPUT */
#define VM_CALL_BATCHING

/* #include "duration_hist.h" */

/* # include <x86intrin.h> */
/* static inline */
/* uint64_t readTSC(void) { */
/*     // _mm_lfence();  // optionally wait for earlier insns to retire before reading the clock */
/*     uint64_t tsc = __rdtsc(); */
/*     // _mm_lfence();  // optionally block later instructions until rdtsc retires */
/*     return tsc; */
/* } */


static void kick_tx(struct xsk_socket_info *xsk)
{
	int ret;

	ret = sendto(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
	if (ret >= 0 || errno == ENOBUFS || errno == EAGAIN ||
			errno == EBUSY || errno == ENETDOWN)
		return;
	ERROR("in kick_tx!\n");
	exit(EXIT_FAILURE);
}

void complete_tx(struct xsk_socket_info *xsk) {
	if (!xsk->outstanding_tx)
		return;

	/* if (config.copy_mode == XDP_COPY || */
	/* 		xsk_ring_prod__needs_wakeup(&xsk->tx)) { */
	if (config.copy_mode == XDP_COPY) {
		/* xsk->app_stats.copy_tx_sendtos++; */
		kick_tx(xsk);
	}
	size_t ndescs = (xsk->outstanding_tx > config.batch_size) ?
		config.batch_size : xsk->outstanding_tx;

	uint32_t idx_cq = 0;
	uint32_t idx_fq = 0;

	/* put back completed Tx descriptors */
	const uint32_t rcvd = xsk_ring_cons__peek(&xsk->umem->cq, ndescs, &idx_cq);
	if (rcvd > 0) {
		unsigned int i;
		int ret;

		ret = xsk_ring_prod__reserve(&xsk->umem->fq, rcvd, &idx_fq);
		while (ret != rcvd) {
			if (ret < 0) {
				ERROR("Error: [complete_tx: parse.c] reserving fill ring failed\n");
				exit(EXIT_FAILURE);
			}

			if (config.busy_poll || xsk_ring_prod__needs_wakeup(&xsk->umem->fq)) {
				/* xsk->app_stats.fill_fail_polls++; */
				recvfrom(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT, NULL,
						NULL);
			}
			ret = xsk_ring_prod__reserve(&xsk->umem->fq, rcvd, &idx_fq);
		}

		for (i = 0; i < rcvd; i++)
			*xsk_ring_prod__fill_addr(&xsk->umem->fq, idx_fq++) =
				*xsk_ring_cons__comp_addr(&xsk->umem->cq, idx_cq++);

		xsk_ring_prod__submit(&xsk->umem->fq, rcvd);
		xsk_ring_cons__release(&xsk->umem->cq, rcvd);
		xsk->outstanding_tx -= rcvd;
		/* xsk->ring_stats.tx_npkts += rcvd; */
	}
}

/**
 * Read descriptors from Rx queue
 *
 * @param xsk Socket to read from
 * @param batch An array to be filled with descriptors
 * @param cnt The batch size
 *
 * @return The number of packets read from the Rx queue
 */
static inline __attribute__((__always_inline__)) uint16_t
poll_rx_queue(struct xsk_socket_info *xsk, struct xdp_desc **batch,
    uint32_t cnt)
{
    uint32_t i;
    uint32_t idx_rx = 0;
    const uint32_t rcvd = xsk_ring_cons__peek(&xsk->rx, cnt, &idx_rx);
    if (!rcvd) {
        /* // There is no packets */
        if (config.busy_poll ||
                xsk_ring_prod__needs_wakeup(&xsk->umem->fq)) {
            /* xsk->app_stats.rx_empty_polls++; */
            recvfrom(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT,
                    NULL, NULL);
        }
        return 0;
    }
    for (i = 0; i < rcvd; i++) {
        struct xdp_desc *desc =
            (struct xdp_desc *)xsk_ring_cons__rx_desc(&xsk->rx, idx_rx);
        idx_rx++;
        batch[i] = desc;
    }
    xsk_ring_cons__release(&xsk->rx, rcvd);
    /* xsk->ring_stats.rx_npkts += rcvd; */
    return rcvd;
}

/**
 * Places cnt number of packets from batch to fill queue
 *
 * @param xsk Socket to work with
 * @param batch The batch of descriptors which are placed into the fill queue
 * @param cnt The batch size
 *
 * @return Number of packets placed into the fill queue. Zero if failed
 * otherwise it would be equal to cnt.
 */
uint32_t drop(struct xsk_socket_info *xsk, struct xdp_desc **batch, uint32_t cnt)
{
    uint32_t idx_target = 0;
    uint32_t i;
    int ret;
    ret = xsk_ring_prod__reserve(&xsk->umem->fq, cnt, &idx_target);
    if (ret != cnt) {
        if (ret < 0) {
            ERROR("Failed to reserve packets on fill queue!\n");
            exit(EXIT_FAILURE);
        }
        return 0;
    }

    for (i = 0; i < cnt; i++) {
        // Index of descriptors in the umem
        uint64_t orig = xsk_umem__extract_addr(batch[i]->addr);
        *xsk_ring_prod__fill_addr(&xsk->umem->fq, idx_target) = orig;
        idx_target++;
    }
    xsk_ring_prod__submit(&xsk->umem->fq, cnt);
    return cnt;
}

/**
 * Places cnt number of packets from batch to tx queue
 *
 * @param xsk Socket to work with
 * @param batch The batch of descriptors
 * @param cnt The batch size
 *
 * @return Number of packets placed into the fill queue. Zero if failed
 * otherwise it would be equal to cnt.
 */
uint32_t tx(struct xsk_socket_info *xsk, struct xdp_desc **batch, uint32_t cnt)
{
    uint32_t idx_target = 0;
    uint32_t i;
    int ret;
    ret = xsk_ring_prod__reserve(&xsk->tx, cnt, &idx_target);
    if (ret != cnt) {
        if (ret < 0) {
            ERROR("Failed to reserve packets on fill queue!\n");
            exit(EXIT_FAILURE);
        }
        return 0;
    }

    for (i = 0; i < cnt; i++) {
        // Index of descriptors in the umem
        uint64_t orig = xsk_umem__extract_addr(batch[i]->addr);
        xsk_ring_prod__tx_desc(&xsk->tx, idx_target)->addr = orig;
        xsk_ring_prod__tx_desc(&xsk->tx, idx_target)->len = batch[i]->len;
        idx_target++;
    }
    xsk->outstanding_tx += cnt;
    xsk_ring_prod__submit(&xsk->tx, cnt);
    return cnt;
}

void
apply_action(struct xsk_socket_info *xsk, struct xdp_desc *desc, int action)
{
	/* DEBUG("action: %d\n", action); */
	int ret;
	// TODO: Implement the list of actions (DROP, TX, ...)
	if (action == SEND) {
		/* ret = tx(xsk, &desc, 1); */
		ret = tx(xsk, &desc, 1);
		if (ret != 1) {
			DEBUG("Failed to send packet\n");
			drop(xsk, &desc, 1);
		}
	} else if (action == PASS) {
		/* Send to application using the interpose layer */
		uint64_t addr = desc->addr;
		addr = xsk_umem__add_offset_to_addr(addr);
		void *ctx = xsk_umem__get_data(xsk->umem->buffer, addr);
		/* Note: On the other side interpose.c cxpects both IP and UDP
		 * headers to be provided to answer "recvfrom" request. */
		int ret = send_interpose_msg(ctx, desc->len);
		if (ret < 0) {
			// failed to pass packet
			DEBUG("Failed to pass packect");
		}
		// Free the AF_XDP descriptor
		drop(xsk, &desc, 1);
	} else {
		drop(xsk, &desc, 1);
	}
}

void
apply_mix_action(struct xsk_socket_info *xsk, struct xdp_desc **batch,
		struct pktctxbatch *ctx_batch, uint32_t cnt)
{
	/* DEBUG("apply_mix_action\n"); */
	int ret;
	// 0 drop, 1 tx, 2 pass
	int action_count[3] = {};
	uint32_t index_target[3] = {};
	int reserved[3] = {};
	struct xsk_ring_prod *rings[3] = {};
	rings[0] = &xsk->umem->fq;
	rings[1] = &xsk->tx;

	// Count each action and prepare the descriptors
	for (int i = 0; i < cnt; i++) {
		batch[i]->len = ctx_batch->pkts[i].pkt_len;
		batch[i]->addr += ctx_batch->pkts[i].trim_head;
		if (ctx_batch->rets[i] == DROP) {
			action_count[0]++;
		} else if (ctx_batch->rets[i] == SEND) {
			action_count[1]++;
		} else {
			// not implemented yet
			continue;
		}
	}
	/* DEBUG("action count 0: %d 1: %d\n", action_count[0], action_count[1]); */

	// Try to reserve space on rings
	for (int i = 0; i < 2; i++) {
		if (action_count[i] < 1) {
			continue;
		}
		ret = xsk_ring_prod__reserve(rings[i], action_count[i], &index_target[i]);
		if (ret != action_count[i]) {
			if (ret < 0) {
				ERROR("Failed to reserve packets on queue!\n");
				exit(EXIT_FAILURE);
			}
			ERROR("Failed to reserve space on queue\n");
			continue;
		}
		reserved[i] = 1;
		/* DEBUG("reserve action %d: %d\n", i, action_count[i]); */
	}

	// Place descriptors on rings
	for (int i = 0; i < cnt; i++) {
		uint64_t orig = xsk_umem__extract_addr(batch[i]->addr);
		if (ctx_batch->rets[i] == DROP) {
			*xsk_ring_prod__fill_addr(rings[0], index_target[0]) = orig;
			index_target[0]++;
		} else if (ctx_batch->rets[i] == SEND) {
			// Index of descriptors in the umem
			xsk_ring_prod__tx_desc(rings[1], index_target[1])->addr = orig;
			xsk_ring_prod__tx_desc(rings[1], index_target[1])->len = batch[i]->len;
			index_target[1]++;
		} else {
			// Not implemented
			continue;
		}
	}

	xsk->outstanding_tx += action_count[1];
	// Submit queue
	for (int i = 0; i < 2; i++) {
		if (reserved[i] > 0)
			xsk_ring_prod__submit(rings[i], action_count[i]);
	}
}

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
	clock_gettime(CLOCK_REALTIME, &spec);
	uint64_t rprt_ts = spec.tv_sec * 1000000 + spec.tv_nsec / 1000;
#endif

	uint32_t rx;
	const uint32_t cnt = config.batch_size;
	struct xdp_desc *batch[cnt];
#ifdef VM_CALL_BATCHING
	struct pktctx _pkt_ctx_arr[cnt];
	int _ret_arr[cnt];
	struct pktctxbatch pkt_batch = {};
	pkt_batch.pkts = _pkt_ctx_arr;
	pkt_batch.rets = _ret_arr;
#else
	struct pktctx pktctx;
#endif

	if (!config.jitted) {
		INFO("Intentionally use jitted mode (ignore the flag)\n");
	}
	char *errmsg;
	ubpf_jit_fn fn = ubpf_compile(vm, &errmsg);
	if (fn == NULL) {
		ERROR("Failed to compile: %s\n", errmsg);
		free(errmsg);
		return;
	}

	for(;;) {
		if (config.terminate)
			break;

		if (xsk->outstanding_tx > 0)
			complete_tx(xsk);

		rx = poll_rx_queue(xsk, batch, cnt);
		if (!rx)
			continue;

#ifdef VM_CALL_BATCHING
		/* Perpare batch */
		pkt_batch.cnt = rx;
		for (int i = 0; i < rx; i++) {
			uint64_t addr = batch[i]->addr;
			addr = xsk_umem__add_offset_to_addr(addr);
			size_t ctx_len = batch[i]->len;
			void *ctx = xsk_umem__get_data(xsk->umem->buffer, addr);
			pkt_batch.pkts[i].data = ctx;
			pkt_batch.pkts[i].data_end = ctx + ctx_len;
			pkt_batch.pkts[i].pkt_len = ctx_len;
			pkt_batch.pkts[i].trim_head = 0;
			pkt_batch.rets[i] = 0;
		}
		/* Pass batch to the vm */
		/* uint64_t start_ts = readTSC(); */
		fn(&pkt_batch, sizeof(pkt_batch));
		/* uint64_t end_ts = readTSC(); */
		/* calc_latency_from_ts(start_ts, end_ts); */
		apply_mix_action(xsk, batch, &pkt_batch, rx);
#ifdef SHOW_THROUGHPUT
		for (int i = 0; i < rx; i++)
			if (pkt_batch.rets[i] == SEND)
				sent_count++;
#endif
#else
		for (int i = 0; i < rx; i++) {
			uint64_t addr = batch[i]->addr;
			addr = xsk_umem__add_offset_to_addr(addr);
			size_t ctx_len = batch[i]->len;
			void *ctx = xsk_umem__get_data(xsk->umem->buffer, addr);
			pktctx.data = ctx;
			pktctx.data_end = ctx + ctx_len;
			pktctx.pkt_len = ctx_len;
			pktctx.trim_head = 0;
			/* uint64_t start_ts = readTSC(); */
			int ret = fn(&pktctx, sizeof(pktctx));
			/* uint64_t end_ts = readTSC(); */
			/* calc_latency_from_ts(start_ts, end_ts); */
			batch[i]->len = pktctx.pkt_len;
			/* batch[i]->addr += pktctx.trim_head; */
			apply_action(xsk, batch[i], ret);
#ifdef SHOW_THROUGHPUT
			/* DEBUG("action: %d\n", ret); */
			if (ret == SEND)
				sent_count++;
#endif
		}
#endif

#ifdef SHOW_THROUGHPUT
		pkt_count += rx;
		/* if (ret == SEND) */
		/* 	sent_count++; */
		clock_gettime(CLOCK_REALTIME, &spec);
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
	/* print_latency_result(); */
}
