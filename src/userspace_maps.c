#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>

#include "log.h"
#include "userspace_maps.h"

#define MAX 2048
#define PORT 8080
#define MAX_CONCURRENT_CONNECTION 1

/* void handle_connection(int connfd) */
/* { */
/* 	char read_buff[MAX + 1]; */
/* 	char write_buff[MAX + 1]; */
/* 	int n; */
/* 	struct cmd *cmd; */
/* 	int ret = 0; */

/* 	/1* Loop until terminate command or connection close *1/ */
/* 	for (;;) { */
/* 		bzero(read_buff, MAX); */

/* 		// read the message from client and copy it in buffer */
/* 		ret = read(connfd, read_buff, sizeof(read_buff)); */
/* 		if (ret < 0) */
/* 			break; */

/* 		cmd = (struct cmd *)read_buff; */

/* 		DEBUG("Receive: cmd: %d\n", cmd->cmdid); */

/* 		/1* bzero(buff, MAX); *1/ */
/* 		/1* n = 0; *1/ */
/* 		// copy server message in the buffer */

/* 		// and send that buffer to client */
/* 		/1* write(connfd, buff, sizeof(buff)); *1/ */

/* 		// if msg contains "Exit" then server exit and chat ended. */
/* 		if (cmd->cmdid == EXIT) { */
/* 			INFO("Closing connection"); */
/* 			break; */
/* 		} */
/* 	} */
/* } */

/* void *listener(void *args) */
/* { */
/* 	struct server_conf *config = args; */
/* 	int sockfd; */
/* 	int connfd; */
/* 	int len; */
/* 	struct sockaddr_in servaddr; */
/* 	struct sockaddr_in cli; */

/* 	// socket create */
/* 	sockfd = socket(AF_INET, SOCK_DGRAM, 0); */
/* 	if (sockfd == -1) { */
/* 		ERROR("socket creation failed...\n"); */
/* 		exit(1); */
/* 	} */
/* 	bzero(&servaddr, sizeof(servaddr)); */

/* 	// assign IP, PORT */
/* 	servaddr.sin_family = AF_INET; */
/* 	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); */
/* 	servaddr.sin_port = htons(PORT); */

/* 	// Binding newly created socket to given IP */
/* 	if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) { */
/* 		ERROR("socket bind failed...\n"); */
/* 		exit(1); */
/* 	} */

/* 	// Now server is ready to listen */
/* 	if ((listen(sockfd, MAX_CONCURRENT_CONNECTION)) != 0) { */
/* 		ERROR("Listen failed...\n"); */
/* 		exit(1); */
/* 	} */
/* 	len = sizeof(cli); */

/* 	while(config->running) { */
/* 		// Accept the data packet from client */
/* 		connfd = accept(sockfd, (struct sockaddr*)&cli, &len); */
/* 		if (connfd < 0) { */
/* 			ERROR("server accept failed...\n"); */
/* 			exit(1); */
/* 		} */

/* 		/1* This server can only handle one connection and is very blocking *1/ */
/* 		handle_connection(connfd); */
/* 		close(sockfd); */
/* 	} */
/* } */


// vip's definition for lookup
struct vip_definition {
	union {
		uint32_t vip;
		uint32_t vipv6[4];
	};
	uint16_t port;
	uint8_t proto;
};

// result of vip's lookup
struct vip_meta {
	uint32_t flags;
	uint32_t vip_num;
};

struct real_definition {
  union {
    uint32_t dst;
    uint32_t dstv6[4];
  };
  uint8_t flags;
};

// value for ctl array, could contain e.g. mac address of default router
// or other flags
struct ctl_value {
  union {
    uint64_t value;
    uint32_t ifindex;
    uint8_t mac[6];
  };
};

int launch_userspace_maps_server(struct server_conf *config)
{
	/* pthread_t t; */
	/* return pthread_create(&t, NULL, listener, (void *)config); */

	/* Initialize maps HARDCODED */
	struct ubpf_map *m;
	void *p = NULL;
	int zero = 0;

	/* For a test */
	m = ubpf_select_map("test_map", config->vm);
	if (m) {
		int key = 365;
		int val = 7460;
		ubpf_update_map(m, &key, &val);
	}

	/* for Katran */
	/* Initialize stats */
	m = ubpf_select_map("stats", config->vm);
	if (!m) {
		INFO("Warning: stats map not found!\n");
		return 1;
	}
	p = ubpf_lookup_map(m, &zero);
	INFO("stats[0] %p\n", p);

	/* Add a new vip */
	m = ubpf_select_map("vip_map", config->vm);
	if (!m) {
		INFO("Warning: vip_map map not found!\n");
		return 1;
	}
	struct vip_definition vdef = {};
	vdef.vip = htonl(0xc0a8010a); // 192.168.1.10
	vdef.port = htons(8080);
	vdef.proto = IPPROTO_UDP;
	const int vip_num = 0;
	/* const int flags = 1; // NO_SPORT (reading from grpc client) */
	const int flags = 0; // TODO: what should it be?
	struct vip_meta meta;
	meta.vip_num = vip_num;
	meta.flags = flags;
	ubpf_update_map(m, &vdef, &meta);
	INFO("Add vip %x %d %d\n", vdef.vip, vdef.port, vdef.proto);
	/* void *p = ubpf_lookup_map(m, &vdef); */
	/* INFO("looking up the new pointer %p\n", p); */

	/* Add a new real */
	m = ubpf_select_map("reals", config->vm);
	if (!m) {
		INFO("Warning: reals map not found!\n");
		return 1;
	}
	struct real_definition real = {
		.dst = htonl(0xc0a80101), // 192.168.1.1
		.flags = 0, // TODO: what should it be?
	};
	int real_num = 0;
	ubpf_update_map(m, &real_num, &real);
	INFO("Add a real 192.168.1.1\n");

	/* Add a real to vip */
	m = ubpf_select_map("ch_rings", config->vm);
	if (!m) {
		INFO("Warning: ch_rings map not found!\n");
		return 1;
	}
	const int ch_ring_size = 65537;
	const int pos = 0;
	int ch_rings_key = vip_num * ch_ring_size + pos;
	ubpf_update_map(m, &ch_rings_key, &real.dst);
	INFO("Add real to vip\n");

	/* Configure default MAC address */
	m = ubpf_select_map("ctl_array", config->vm);
	if (!m) {
		INFO("Warning: ctl_array map not found!\n");
		return 1;
	}
	int ctl_array_index = 0;
	struct ctl_value ctl_val = {
		.mac = {0x3c,0xfd,0xfe,0x56,0x05,0x42},
	};
	ubpf_update_map(m, &ctl_array_index, &ctl_val);
	INFO("Updated default mac address\n");

	return 0;
}
