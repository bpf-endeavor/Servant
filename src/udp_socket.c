#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "udp_socket.h"
#include "log.h"

#define BUFLEN 2048
static int socket_fd = 0;
static struct sockaddr_in dest = {};

int setup_udp_socket(void)
{
	int ret;
	int fd;
	int on = 1;

	fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (fd < 1) {
		ERROR("Failed to create UDP socket\n");
		return -1;
	}
	setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

	// Hardcoding Memcached address
	dest.sin_family = AF_INET;
	dest.sin_port = htons(8080);
	// 192.168.1.1
	ret = inet_aton("192.168.1.1" , &dest.sin_addr);
	if (ret == 0) {
		ERROR("Failed to set udp socket dest address");
		return -1;
	}

	socket_fd = fd;
	return 0;
}

int teardown_udp_socket(void)
{
	close(socket_fd);
	return 0;
}

int send_udp_socket_msg(void *buf, uint32_t size)
{
	const int flags = 0;
	return sendto(socket_fd, buf, size, flags, (struct sockaddr *)&dest,
			sizeof(dest));
}
