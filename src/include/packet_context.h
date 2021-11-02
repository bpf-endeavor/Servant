#ifndef PACKET_CONTEXT_H
#define PACKET_CONTEXT_H


// Return values
#define PASS 100
#define DROP 200
#define SEND 300

// Context parameter type
struct pktctx {
	void *data;
	void *data_end;
	size_t pkt_len;
};

#endif
