#ifndef __XSK_ACTIONS_H
#define __XSK_ACTIONS_H
#include "config.h"
#include "defs.h"

#define __inline static inline __attribute__((always_inline))
#define MAX(a,b) (((a) < (b)) ? (b) : (a))
#define CHUNK_ALIGN(a) ((a) & DEFAULT_CHUNK_MASK)

__inline void kick_tx(struct xsk_socket_info *xsk)
{
	int ret;
	if (config.copy_mode != XDP_COPY &&
			!xsk_ring_prod__needs_wakeup(&xsk->tx)) {
		return;
	}
	xsk->app_stats.copy_tx_sendtos++;
	ret = sendto(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT,
			NULL, 0);
	if (ret >= 0 || errno == ENOBUFS || errno == EAGAIN ||
			errno == EBUSY || errno == ENETDOWN)
		return;
	ERROR("in kick_tx!\n");
	exit(EXIT_FAILURE);
}

__inline void kick_rx(struct xsk_socket_info *xsk)
{
	if (config.busy_poll || xsk_ring_prod__needs_wakeup(&xsk->umem->fq)) {
		xsk->app_stats.rx_empty_polls++;
		recvfrom(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT,
				NULL, NULL);
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
__inline uint32_t poll_rx_queue(struct xsk_socket_info *xsk,
		struct xdp_desc *batch, uint32_t cnt)
{
	uint32_t i;
	uint32_t idx_rx;

	const uint32_t rcvd = xsk_ring_cons__peek(&xsk->rx, cnt, &idx_rx);
	if (!rcvd) {
		kick_rx(xsk);
		return 0;
	}
	for (i = 0; i < rcvd; i++)
		batch[i] = *xsk_ring_cons__rx_desc(&xsk->rx, idx_rx++);
	/* xsk->ring_stats.rx_npkts += rcvd; */
	return rcvd;
}

__inline void release_rx_queue(struct xsk_socket_info *xsk,
		const uint32_t cnt)
{
	xsk_ring_cons__release(&xsk->rx, cnt);
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
/* __inline uint32_t drop(struct xsk_socket_info *xsk, */
/* 		const struct xdp_desc *batch, const uint32_t cnt) */
/* { */
/* 	uint32_t idx_target = 0; */
/* 	uint32_t i; */
/* 	uint32_t ret; */
/* 	uint64_t *fqd; */

/* 	do { */
/* 		ret = xsk_ring_prod__reserve(&xsk->umem->fq, cnt, &idx_target); */
/* 		if (ret != cnt) { */
/* 			kick_rx(xsk); */
/* 		} */
/* 	} while(ret != cnt); */

/* 	for (i = 0; i < cnt; i++) { */
/* 		fqd = xsk_ring_prod__fill_addr(&xsk->umem->fq, idx_target++); */
/* 		*fqd = CHUNK_ALIGN(batch[i].addr); */
/* 	} */
/* 	xsk_ring_prod__submit(&xsk->umem->fq, cnt); */
/* 	/1* xsk_ring_cons__release(&xsk->rx, cnt); *1/ */
/* 	return cnt; */
/* } */

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
/* __inline uint32_t tx(struct xsk_socket_info *xsk, const struct xdp_desc *batch, */
/* 		const uint32_t cnt) */
/* { */
/* 	uint32_t idx_target = 0; */
/* 	uint32_t i = 0; */
/* 	int ret = 0; */
/* 	do { */
/* 		ret = xsk_ring_prod__reserve(&xsk->tx, cnt, &idx_target); */
/* 		if (ret != cnt) { */
/* 			kick_tx(xsk); */
/* 		} */
/* 	} while(ret != cnt); */

/* 	for (i = 0; i < cnt; i++) */
/* 		*xsk_ring_prod__tx_desc(&xsk->tx, idx_target++) = batch[i]; */
/* 	xsk->outstanding_tx += cnt; */
/* 	xsk_ring_prod__submit(&xsk->tx, cnt); */
/* 	/1* xsk_ring_cons__release(&xsk->rx, cnt); *1/ */
/* 	return cnt; */
/* } */

/* Service the completion queue. Move the descriptors successfully transmitted
 * by the kernel to the fill queue for receiving new incomming requests.
 * */
__inline int complete_tx(struct xsk_socket_info *xsk)
{
	if (!xsk->outstanding_tx)
		return 0;

	uint32_t ndescs = MAX(config.batch_size, xsk->outstanding_tx);
	uint32_t idx_cq = 0;
	uint32_t idx_fq = 0;
	uint64_t *cqd, *fqd;

	/* put back completed Tx descriptors */
	const uint32_t rcvd = xsk_ring_cons__peek(&xsk->umem->cq, ndescs, &idx_cq);
	if (rcvd == 0) {
		/* Okay, we have some outstanding, but kernel has not done
		 * anything. It is fine. we wait */
		kick_tx(xsk);
		return 0;
	}

	uint32_t ret = 0;
	do {
		ret = xsk_ring_prod__reserve(&xsk->umem->fq, rcvd, &idx_fq);
		if (ret != rcvd) {
			kick_rx(xsk);
		}
	} while (ret != rcvd);

	for (uint32_t i = 0; i < rcvd; i++) {
		cqd = xsk_ring_cons__comp_addr(&xsk->umem->cq, idx_cq++);
		fqd = xsk_ring_prod__fill_addr(&xsk->umem->fq, idx_fq++);
		*fqd = CHUNK_ALIGN(*cqd);
	}

	xsk_ring_prod__submit(&xsk->umem->fq, rcvd);
	xsk_ring_cons__release(&xsk->umem->cq, rcvd);
	xsk->outstanding_tx -= rcvd;
	/* xsk->ring_stats.tx_npkts += rcvd; */
	return ret;
}
#endif
