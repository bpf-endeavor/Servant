#ifndef _UBENCH_DEFS_H
#define _UBENCH_DEFS_H
#include <stdint.h>
#include <bpf/xsk.h>

#define SO_PREFER_BUSY_POLL     69
#define SO_BUSY_POLL_BUDGET     70

struct xsk_ring_stats {
    unsigned long rx_npkts;
    unsigned long tx_npkts;
    unsigned long rx_dropped_npkts;
    unsigned long rx_invalid_npkts;
    unsigned long tx_invalid_npkts;
    unsigned long rx_full_npkts;
    unsigned long rx_fill_empty_npkts;
    unsigned long tx_empty_npkts;
};

struct xsk_driver_stats {
    unsigned long intrs;
};

struct xsk_app_stats {
    unsigned long rx_empty_polls;
    unsigned long fill_fail_polls;
    unsigned long copy_tx_sendtos;
    unsigned long tx_wakeup_sendtos;
    unsigned long opt_polls;
};

struct xsk_umem_info {
    struct xsk_ring_prod fq;
    struct xsk_ring_cons cq;
    struct xsk_umem *umem;
    void *buffer;
};

// For the current batch that we are processing, store the following info:
struct batch_stats {
	// number of packets going to tx queue
	uint32_t tx;
	// number of packets going to fill queue
	uint32_t drop;
};

struct xsk_socket_info {
    struct xsk_ring_cons rx;
    struct xsk_ring_prod tx;
    struct xsk_umem_info *umem;
    struct xsk_socket *xsk;
    struct xsk_ring_stats ring_stats;
    struct xsk_app_stats app_stats;
    struct xsk_driver_stats drv_stats;
    unsigned int outstanding_tx;
    struct batch_stats batch;
};
#endif
