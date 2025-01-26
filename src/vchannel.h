#ifndef VQUEUE_H
#define VQUEUE_H
#include <stdint.h>
#include <pthread.h>
#include "llring.h"

#define COUNT_RINGS 2


#define MAX_SHARED_OBJ_SIZE 1500
/**
 * Internal representation of messages stored in shared memory
 */
struct _interpose_msg {
  uint8_t data[MAX_SHARED_OBJ_SIZE];
  uint32_t size;
  uint32_t index;
  uint32_t valid;
} __attribute__((packed,aligned(64)));

/**
 * This struct is the layout of the shared memory region
 */
struct _shared_channel {
  uint64_t master_virt_addr; // what is the start of virtual address
  uint64_t region_size; // Total size of the shared memory
  int connected; // allow at most two objects to connect
  uint32_t ring_size; // slots in a ring
  /* uint32_t in; */
  /* uint32_t out; */
  struct llring *rings[COUNT_RINGS]; // rings[i] is for messages that are send to proc_i
  /* pthread_spinlock_t lock; */
  size_t count_elements; // Count elements in obj pool (also stack size)
  /* int stack_top; // index of the top element in stack */
  /* uint32_t *index_stack; */
  uint32_t ring_top;
  struct _interpose_msg *shared_objs; // shared objects are placed here
};

/**
 * A non-shared object for accessing the shared channel
 */
struct vchannel {
  char *name; // name of the queue/shared region
  int proc_id;
  struct _shared_channel *ch;
};

/**
 * Information used for creating or connecting to a channel
 */
struct channel_attr {
  char *name;
  uint32_t ring_size;
};

/**
 * Creates a new shared channel or attaches to an existing one.
 * The name parameter is used for naming and or finding the
 * shared memory region.
 * This function tries to map the shared region to the same virtual address
 * in both processes. This way pointers can work in the shared region.
 *
 * TODO: if it fails to map to the same region, then what to do?
 *
 * @param name name of the channel
 * @param vc this object is filled and initialized if the function
 * succedes.
 * @returns zero on success.
 */
int connect_shared_channel(struct channel_attr *attr, struct vchannel *vc);

/**
 * Send messages on the channel
 *
 * First copies the message to the shared memory region
 */
int vc_tx_msg(struct vchannel *vc, void *buf, uint32_t size);

/**
 * Recv messages from the channel
 *
 * Copies message from shared memory region to the given buffer
 */
int vc_rx_msg(struct vchannel *vc, void *buf, uint32_t size);

/**
 * @returns the number of messages waiting in queue
 */
int vc_count_msg(struct vchannel *vc);

/**
 * Disconnect from channel
 */
int disconnect(struct vchannel *vc);

#endif
