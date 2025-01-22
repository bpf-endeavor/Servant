#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <assert.h>
#include <arpa/inet.h>

#include <ubpf_maps.h>

#include "log.h"
#include "userspace_maps.h"
#include "map.h"

#define MAX 2048
#define PORT 8080
#define MAX_CONCURRENT_CONNECTION 1

/* TODO: use grpc to quickly bring up a server that can configure tha maps */

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


struct flow_key {
	union {
		uint32_t src;
		uint32_t srcv6[4];
	};
	union {
		uint32_t dst;
		uint32_t dstv6[4];
	};
	union {
		uint32_t ports;
		uint16_t port16[2];
	};
	uint8_t proto;
};

struct real_pos_lru {
	uint32_t pos;
	uint64_t atime;
};

struct vip_definition {
	union {
		uint32_t vip;
		uint32_t vipv6[4];
	};
	uint16_t port;
	uint8_t proto;
};

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

static void __configure_katran_maps(void *vm)
{
	int ret;
	int vip, dst;
	short vip_port = 8080;
	uint8_t vip_proto = IPPROTO_UDP;
	ret = inet_pton(AF_INET, "192.168.1.1", &vip);
	assert (ret == 1);
	ret = inet_pton(AF_INET, "192.168.1.2", &dst);
	assert (ret == 1);

	/* TODO: add support for map-of-map */
	/* Prepare the lookup table for stateful routing
	 * Create one hash map (instead of the LRU map used in katran)
	 * Insert it into the map-of-maps at index 0
	 * */
	uint64_t zero = 0;
	/* struct ubpf_map_def lru_map_def = { */
	/* 	.type = UBPF_MAP_TYPE_HASHMAP, /1* TODO: katran uses LRU map *1/ */
	/* 	.key_size = sizeof(struct flow_key), */
	/* 	.value_size = sizeof(struct real_pos_lru), */
	/* 	.max_entries = 8000000, */
	/* 	.nb_hash_functions = 1, */
	/* }; */
	/* void *lru_map = ubpf_create_map("_lru_map_0", &lru_map_def, vm); */
	/* assert (lru_map != NULL); */
	/* void *lru_mapping = ubpf_select_map("lru_mapping", vm); */
	/* assert(lru_mapping != NULL); */
	/* ret = ubpf_update_map(lru_mapping, &zero, lru_map); */
	/* /1* printf("%d\n", ret); *1/ */
	/* assert (ret == 0); */

	/* Configure the routing info */
	void *vip_map = ubpf_select_map("vip_map", vm);
	assert (vip_map != NULL);
	struct vip_definition vipdef = {};
	vipdef.vip = vip;
	vipdef.port = htons(vip_port);
	vipdef.proto = vip_proto;
	struct vip_meta vipmeta = {
		.flags = 0,
		.vip_num = 0,
	};
	ret = ubpf_update_map(vip_map, &vipdef, &vipmeta);
	assert (ret == 0);
	void *m = ubpf_lookup_map(vip_map, &vipdef);
	assert (m != NULL);

	void *ch_rings = ubpf_select_map("ch_rings", vm);
	assert (ch_rings != NULL);
	ret = ubpf_update_map(ch_rings, &zero, &zero);
	assert(ret == 0);

	struct real_definition realdef = {
		.dst = dst,
		.flags = 0,
	};
	void *reals = ubpf_select_map("reals", vm);
	assert (reals != NULL);
	ret = ubpf_update_map(reals, &zero, &realdef);
	assert (ret == 0);
	printf("update katran maps!\n");
}

int launch_userspace_maps_server(struct server_conf *config)
{
	/* TODO: create a service with REST or GRPC api for modifying the userspace maps */
	/* pthread_t t; */
	/* return pthread_create(&t, NULL, listener, (void *)config); */

	/* Example of accessing the userspace maps from control plane */
	/*
	struct ubpf_map *m;
	void *p = NULL;
	int zero = 0;

	m = ubpf_select_map("test_map", config->vm);
	if (m) {
		int key = 365;
		int val = 7460;
		ubpf_update_map(m, &key, &val);
	}
	*/

	__configure_katran_maps(config->vm);

	return 0;
}
