#ifndef SOCKETS_H
#define SOCKETS_H
#include "defs.h"

#define MAX_QID 63

/**
 * Create a new socket, allocate a new umem and setup queues
 *
 * @param ifname Interface name
 * @param qid Queue number
 *
 * @return Returns a pointer to a socket info structure.
 */
struct xsk_socket_info * setup_socket(char *ifname, uint32_t qid);

/**
 * Destroy a socket info structure
 *
 * @param xsk Socket to be destroyed.
 */
void tear_down_socket(struct xsk_socket_info *xsk);

/**
 * Load an XDP program. If there is already another program attach will fail.
 *
 * @return Returns zero on success.
 */
int load_xdp_program(char *xdp_filename, int ifindex);

/**
 * Add xsk socket to the xsks_map so that XDP redirect
 * packets to this socket.
 *
 * Note: Expects the XDP program to have a map named "xsks_map" that is used for
 * redirecting traffic to AF_XDP.
 *
 * @param xsk Socket that is added to the map (receiving traffic on qid).
 * @param qid Rx-queue number that this socket is connected to.
 *
 * @return Returns zero on success.
 */
int enter_xsks_into_map( struct xsk_socket_info *xsk, int qid);
#endif
