#ifndef SOCKETS_H
#define SOCKETS_H
#include "defs.h"
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
#endif
