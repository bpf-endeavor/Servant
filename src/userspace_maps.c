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

	return 0;
}
