/**
 * This function has the function that interface with AF_XDP. Receiving packets
 * and pumping them to the eBPF engine (Brain)
 */
#include <stdlib.h> // exit
#include "config.h"
#include "heart.h"
#include "brain.h"
#include "log.h"

/**
 * Read descriptors from Rx queue
 *
 * @param xsk Socket to read from
 * @param batch An array to be filled with descriptors
 * @param cnt The batch size
 *
 * @return The number of packets read from the Rx queue
 */
uint32_t
poll_rx_queue(struct xsk_socket_info *xsk, struct xdp_desc **batch,
    uint32_t cnt)
{
    uint32_t i;
    uint32_t idx_rx = 0;
    const uint32_t rcvd = xsk_ring_cons__peek(&xsk->rx, cnt, &idx_rx);
    if (!rcvd) {
        /* // There is no packets */
        /* if (config.busy_poll || */
        /*         xsk_ring_prod__needs_wakeup(&xsk->umem->fq)) { */
        /*     /1* xsk->app_stats.rx_empty_polls++; *1/ */
        /*     recvfrom(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT, */
        /*             NULL, NULL); */
        /* } */
        return rcvd;
    }
    for (i = 0; i < rcvd; i++) {
        const struct xdp_desc *desc =
            xsk_ring_cons__rx_desc(&xsk->rx, idx_rx);
        idx_rx++;
        /* uint64_t addr = desc->addr; */
        /* uint32_t len = desc->len; */
        /* addr = xsk_umem__add_offset_to_addr(addr); */
        /* void *pkt = xsk_umem__get_data(xsk->umem->buffer, addr); */
        batch[i] = desc;
    }
    xsk_ring_cons__release(&xsk->rx, rcvd);
    xsk->ring_stats.rx_npkts += rcvd;
    return rcvd;
}

/**
 * Places cnt number of packets from socket's rx queue to fill queue
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

void
apply_action(struct xsk_socket_info *xsk, struct xdp_desc *desc, int action)
{
	// TODO: Implement the list of actions (DROP, TX, ...)
	drop(xsk, &desc, 1);
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
    uint32_t rx;
    const uint32_t cnt = config.batch_size;
    struct xdp_desc *batch[cnt];
    for(;;) {
        if (config.terminate) {
            break;
        }
        rx = poll_rx_queue(xsk, batch, cnt);
        if (!rx)
            continue;
        /* DEBUG("Rx: %d\n", rx); */

        // TODO: there is no brain yet!
        // drop(xsk, batch, rx);

        // Pass to brain
	for (int i = 0; i < rx; i++) {
		uint64_t addr = batch[i]->addr;
		addr = xsk_umem__add_offset_to_addr(addr);
		size_t ctx_len = batch[i]->len;
		void *ctx = xsk_umem__get_data(xsk->umem->buffer, addr);
		int ret = run_vm(vm, ctx, ctx_len);
		apply_action(xsk, batch[i], ret);
	}
    }
}

