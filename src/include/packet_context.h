#ifndef PACKET_CONTEXT_H
#define PACKET_CONTEXT_H
typedef int64_t verdict_t;

// Return values
#define PASS 100L
#define DROP 200L
#define SEND 300L
#define YIELD 400L

#define SERVANT_METADATA_SIZE 1024
#define SERVANT_INTER_STAGE_STATE_SIZE 128

/* TODO: This is just a hack to pass some info from the Servant to the Katran
 * program.  This can be alleviated  by adding support for global variables for
 * uBPF. 
 * */
typedef struct {
	uint64_t server_id_map_base;
} help_t;

// Context parameter type
struct pktctx {
	void *data; // in: start of the packet
	void *data_end; // in: end of the packet
	void *meta;
	uint32_t pkt_len; // inout: final length of packet
	int32_t trim_head; // out: skip n bytes from the head
	help_t help;
} __attribute__((aligned(16)));

struct pktctxbatch {
	uint32_t cnt; // number of packets in this batch
	struct pktctx *pkts; // points to array of pktctx pointers
	verdict_t *rets; // points to an array of integers
}; 

#endif
