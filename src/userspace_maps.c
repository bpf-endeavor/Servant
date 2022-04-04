#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>

#include <ubpf_maps.h>

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

// flow metadata
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

// where to send client's packet from LRU_MAP
struct real_pos_lru {
  uint32_t pos;
  uint64_t atime;
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

	/* For hash_map bench */
	m = ubpf_select_map("hash_map", config->vm);
	if (m) {
		struct { char obj[8]; } key, val;
		for (int i = 0; i < 128; i++) {
			*(int *)(&key.obj) = i;
			ubpf_update_map(m, &key, &val);
		}
	}

	/* for Katran */
	const int vip_num = 0;
	struct real_definition real = {
		.dst = htonl(0xc0a80101), // 192.168.1.1
		.flags = 0, // TODO: what should it be?
	};
	/* Initialize stats */
	m = ubpf_select_map("stats", config->vm);
	if (m) {
		p = ubpf_lookup_map(m, &zero);
		INFO("stats[0] %p\n", p);
	} else {
		INFO("Warning: stats map not found!\n");
	}

	/* Add a new vip */
	m = ubpf_select_map("vip_map", config->vm);
	if (m) {
		struct vip_definition vdef = {};
		vdef.vip = htonl(0xc0a8010a); // 192.168.1.10
		vdef.port = htons(8080);
		vdef.proto = IPPROTO_UDP;
		/* const int flags = 1; // NO_SPORT (reading from grpc client) */
		const int flags = 0; // TODO: what should it be?
		struct vip_meta meta;
		meta.vip_num = vip_num;
		meta.flags = flags;
		ubpf_update_map(m, &vdef, &meta);
		INFO("Add vip %x %d %d\n", vdef.vip, vdef.port, vdef.proto);
		/* void *p = ubpf_lookup_map(m, &vdef); */
		/* INFO("looking up the new pointer %p\n", p); */
	} else {
		INFO("Warning: vip_map map not found!\n");
	}

	/* Add a new real */
	m = ubpf_select_map("reals", config->vm);
	if (m) {
		int real_num = 0;
		ubpf_update_map(m, &real_num, &real);
		INFO("Add a real 192.168.1.1\n");
	} else {
		INFO("Warning: reals map not found!\n");
	}

	/* Add a real to vip */
	m = ubpf_select_map("ch_rings", config->vm);
	if (m) {
		const int ch_ring_size = 65537;
		const int pos = 0;
		int ch_rings_key = vip_num * ch_ring_size + pos;
		ubpf_update_map(m, &ch_rings_key, &real.dst);
		INFO("Add real to vip\n");
	} else {
		INFO("Warning: ch_rings map not found!\n");
	}

	/* Configure default MAC address */
	m = ubpf_select_map("ctl_array", config->vm);
	if (m) {
		int ctl_array_index = 0;
		struct ctl_value ctl_val = {
			/* .mac = {0x3c,0xfd,0xfe,0x56,0x05,0x42}, */
			/* .mac = {0x3c,0xfd,0xfe,0x56,0x12,0x82}, // Gateway or the other machines mac */
			.mac = {0x3c,0xfd,0xfe,0x56,0x02,0x42}, // Gateway or the other machines mac
		};
		ubpf_update_map(m, &ctl_array_index, &ctl_val);
		INFO("Updated default mac address\n");
	} else {
		INFO("Warning: ctl_array map not found!\n");
	}

	/* Configure LRU mapping */
	/* I am using Hash map instead of LRU_HASH */
	m = ubpf_select_map("lru_mapping", config->vm);
	if (m) {
		int count_cores = 1;
		struct ubpf_map_def map_def = {
			.type = UBPF_MAP_TYPE_HASHMAP, // not LRU?
			.key_size = sizeof(struct flow_key),
			.value_size = sizeof(struct real_pos_lru),
			.max_entries = 10000,
			.nb_hash_functions = 0,
		};
		for (int c = 0; c < count_cores; c++) {
			// TODO: create a new map need uBPF api
			char name[9];
			snprintf(name, 8, "lru%d", c);
			void *new_map = ubpf_create_map(name, &map_def, config->vm);
			ubpf_update_map(m, &c, &new_map);
			INFO("Add a Hash map for core %d\n", c);
		}
	} else {
		INFO("Warning: lru_mapping map not found!\n");
	}

	return 0;
}
