#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "vchannel.h"
#include "log.h"

#define SHM_PATH_MAX_SIZE 256

#define SINGLE_PRODUCER 1
#define SINGLE_CONSUMER 1
#define MULTI_PRODUCER 0
#define MULTI_CONSUMER 0


int
connect_shared_channel(struct channel_attr *attr, struct vchannel *vc)
{
	// Number of slots in a ring
	const int slots = attr->ring_size;
	// Size of a ring in bytes
	size_t ring_size = llring_bytes_with_slots(slots);
	// Size of stack region in bytes
	size_t stack_size = COUNT_RINGS * slots * sizeof(uint32_t);
	// Size of shared object region in bytes
	size_t shared_obj_memory_size = COUNT_RINGS * slots *
		sizeof(struct _interpose_msg);
	size_t shared_region_size = sizeof(struct _shared_channel) +
		COUNT_RINGS * ring_size + stack_size + shared_obj_memory_size;
	int ret;

	void *buf;
	struct _shared_channel *ch;
	char shm_path[SHM_PATH_MAX_SIZE];
	snprintf(shm_path, SHM_PATH_MAX_SIZE - 1, "/servant_%s", attr->name);
	/* Try to create a shared memory object with the given name */
	int shm_fd = shm_open(shm_path, (O_CREAT | O_EXCL | O_RDWR),
			(S_IRUSR | S_IWUSR));
	DEBUG("Try to open obj %s (res: %d)\n", shm_path, shm_fd);
	if (shm_fd > 0) {
		/* This process is the firts to create the object */
		ret = ftruncate(shm_fd, shared_region_size);
		if (ret != 0) {
			shm_unlink(shm_path);
			return -1;
		}
		buf = mmap(NULL, shared_region_size,
				PROT_READ | PROT_WRITE,
				MAP_SHARED, shm_fd, 0);
		if (buf == MAP_FAILED) {
			shm_unlink(shm_path);
			return -1;
		}
		ch = buf;
		ch->master_virt_addr = (uint64_t)buf;
		ch->region_size = shared_region_size;
		ch->connected = 0;
		ch->ring_size = slots;

		/* Initialize the rings */
		for (int i = 0; i < 2; i++) {
			struct llring *r = buf +
				sizeof(struct _shared_channel) +
				i * ring_size;
			ret = llring_init(r, slots, SINGLE_PRODUCER,
					SINGLE_CONSUMER);
			if (ret) {
				munmap(buf, shared_region_size);
				shm_unlink(shm_path);
				return -1;
			}
			ch->rings[i] = r;
		}
		/* Init spin lock */
		pthread_spin_init(&ch->lock, PTHREAD_PROCESS_SHARED);
		/* Initialize the stack region */
		ch->count_elements = COUNT_RINGS * slots;
		ch->stack_top = ch->count_elements - 1;
		ch->index_stack = buf + sizeof(struct _shared_channel) +
			COUNT_RINGS * ring_size;
		for (int i = 0; i < ch->count_elements; i++) {
			ch->index_stack[i] = i;
		}
		/* Initialize shared object memory */
		ch->shared_objs = buf + sizeof(struct _shared_channel) +
			COUNT_RINGS * ring_size + stack_size;
	} else {
		shm_fd = shm_open(shm_path, (O_CREAT | O_RDWR),
				(S_IREAD | S_IWRITE));
		DEBUG("Try to attach to existing region...\n");
		if (shm_fd < 0) {
			return -1;
		}
		struct stat statbuf;
		ret = fstat(shm_fd, &statbuf);
		if (ret != 0) {
			shm_unlink(shm_path);
			return -1;
		}
		DEBUG("region size: %d\n", statbuf.st_size);
		if (statbuf.st_size < shared_region_size) {
			// invalid shared memory!
			shm_unlink(shm_path);
			return -1;
		}

		buf = mmap(NULL, shared_region_size,
				PROT_READ | PROT_WRITE,
				MAP_SHARED, shm_fd, 0);
		if (buf == MAP_FAILED) {
			shm_unlink(shm_path);
			return -1;
		}
		ch = buf;
		DEBUG("mmap successful (connected: %d)\n", ch->connected);
		if (ch->connected >= COUNT_RINGS) {
			// Do not allow more than expacted nubmer of processes
			// to connect.
			munmap(buf, shared_region_size);
			shm_unlink(shm_path);
			return -1;
		}
		uint64_t master_virt_addr = ch->master_virt_addr;
		if ((uint64_t)buf != master_virt_addr) {
			DEBUG("Oh, need to remap to a known virtual address (%p)\n", master_virt_addr);
			ret = munmap(buf, shared_region_size);
			if (ret) {
				shm_unlink(shm_path);
				return -1;
			}
			/* Try to use the same virtual address as the master */
			buf = mmap((void *)master_virt_addr, shared_region_size,
					PROT_READ |  PROT_WRITE,
					MAP_SHARED, shm_fd, 0);
			if (buf == MAP_FAILED) {
				shm_unlink(shm_path);
				return -1;
			}
			DEBUG("Remap successful\n");
			ch = buf;
		}
	}
	vc->proc_id = ch->connected;
	ch->connected++;
	vc->name = attr->name;
	vc->ch = ch;
	return 0;
}

int
vc_tx_msg(struct vchannel *vc, void *buf, uint32_t size)
{
	assert(size < MAX_SHARED_OBJ_SIZE);
	// This only works for when there are only two processes
	int other = 1 - vc->proc_id;
	struct _shared_channel *ch = vc->ch;
	// TODO: Ungaurded critical section
	/* Since there is only a single consumer of indexes it is fine to
	 * check there are some available elements without having the locks */
	if (ch->stack_top < 0) {
		/* No free element to use */
		return -1;
	}
	pthread_spin_lock(&ch->lock);
	int top = ch->stack_top;
	ch->stack_top--; // pop an index
	int index = ch->index_stack[top];
	pthread_spin_unlock(&ch->lock);
	struct _interpose_msg *msg = (ch->shared_objs + index);
	msg->index = index;
	memcpy(msg->data, buf, size);
	// Put pointer in the queue so the receiver can find the data
	int ret = llring_sp_enqueue_burst(ch->rings[other], (void **)&msg, 1);
	ret &= 0x7fffffff;
	return ret;
}

int
vc_rx_msg(struct vchannel *vc, void *buf, uint32_t size)
{
	// This only works for when there are only two processes
	int id = vc->proc_id;
	struct _interpose_msg *msg;
	int ret = llring_sc_dequeue_burst(vc->ch->rings[id],
			(void **)&msg, 1);
	if (ret < 1) {
		// No data to receive
		return 0;
	}
	memcpy(buf, msg->data, msg->size);
	// Ungaurded critical section
	pthread_spin_lock(&vc->ch->lock);
	int top = vc->ch->stack_top;
	vc->ch->index_stack[top + 1] = msg->index;
	vc->ch->stack_top = top + 1;
	pthread_spin_unlock(&vc->ch->lock);
	return msg->size;
}

int
disconnect(struct vchannel *vc)
{
	char shm_path[SHM_PATH_MAX_SIZE];
	int ret = 0;
	snprintf(shm_path, SHM_PATH_MAX_SIZE - 1, "/servant_%s", vc->name);
	vc->ch->connected--;
	shm_unlink(shm_path);
	ret = munmap((void *)(&vc->ch), vc->ch->region_size);
	return ret;
}
