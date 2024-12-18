#ifndef PACKET_CONTEXT_H
#define PACKET_CONTEXT_H

// Return values
#define PASS 100
#define DROP 200
#define SEND 300
#define YIELD 400

#define METADATA_SIZE 1024

// Context parameter type
struct pktctx {
	void *data; // in: start of the packet
	void *data_end; // in: end of the packet
	void *meta;
	uint32_t pkt_len; // inout: final length of packet
	int32_t trim_head; // out: skip n bytes from the head
} __attribute__((aligned(16)));

struct pktctxbatch {
	uint32_t cnt; // number of packets in this batch
	struct pktctx *pkts; // points to array of pktctx pointers
	int *rets; // points to an array of integers
}; 

#endif
