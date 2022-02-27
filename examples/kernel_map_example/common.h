#ifndef COMMON_H
#define COMMON_H

#define MAX_PAYLOAD_SIZE 256
#define NUM_ENTRIES 1024
struct data {
	int ether_type;
	int ip_proto;
	unsigned int len;
	char payload[MAX_PAYLOAD_SIZE];
	int count;
};
#endif
