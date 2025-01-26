#ifndef _UDP_SOCKET_H
#define _UDP_SOCKET_H
#include <stdint.h>
int setup_udp_socket(void);
int teardown_udp_socket(void);
int send_udp_socket_msg(void *buf, uint32_t size);
#endif
