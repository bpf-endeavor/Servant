#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#define __USE_GNU
#include <dlfcn.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <ubpf.h>

#include "../map.h"
#include "../brain.h"
#include "../vchannel.h"

/* --------------------------------------------------------------- */
/* List of functions that are going to override>
 * Uncomment the functions that are needed and build/install again.
 */

/* #define SENDMSG 1 */
#define RECVFROM 1

/* End of the list */
/* --------------------------------------------------------------- */

#define MEM_BARRIER() __asm__ volatile("" ::: "memory")

static inline void ensure_init(void);

#ifdef SENDMSG
// TODO: this is hard coded, maybe communicate with Servant runtime?!
#define UBPF_PROG_PATH "/users/farbod/BMC/bmc/uth.o"

/* The address of jitted function is set here */
static ubpf_jit_fn fn = NULL;

static ssize_t (*libc_sendmsg)(int sockfd, const struct msghdr *msg,
		int flags) = NULL;

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	ensure_init();
	/* printf("hello from userspace tx hook\n"); */
	/* Run eBPF function */
	/* sendmsg promises that it wont change msg, but I might do! */
	fn((struct msghdr *)msg, sizeof(struct msghdr));
	return libc_sendmsg(sockfd, msg, flags);
}
#endif

#ifdef RECVFROM
/* Shared channel */
static char channel_name[] = "rx_data_inject";
static struct vchannel vc;

static ssize_t (*libc_recvfrom)(int sockfd, void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen) = NULL;

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen)
{
	/* printf("recvfrom interposed (before init)\n"); */
	ssize_t ret;
	ensure_init();
	/* printf("recvfrom interposed\n"); */
	/**
	 * First check if there is anything coming from the runtime.
	 * If there is nothing then check the network stack.
	 */
	ret = vc_rx_msg(&vc, buf, len);
	if (ret == 0) {
		return libc_recvfrom(sockfd, buf, len, flags, src_addr,
				addrlen);
	}
	return ret;
}
#endif

/* Helper functions */
static void *bind_symbol(const char *sym)
{
	void *ptr;
	if ((ptr = dlsym(RTLD_NEXT, sym)) == NULL) {
		fprintf(stderr, "fialed to interpose symbol (%s)\n", sym);
		abort();
	}
	return ptr;
}

static void init(void)
{
	int ret;
#ifdef SENDMSG
	libc_sendmsg = bind_symbol("sendmsg");

	/* Setup map system */
	// TODO: it is hardcoded, maybe communicate with Servant runtime?!
	char *names[] = {
		"map_kcache",
		"map_stats",
	};
	const int count = 2;
	setup_map_system(names, count);
	/* Setup eBPF engine */
	struct ubpf_vm *vm;
	ret = setup_ubpf_engine(UBPF_PROG_PATH, &vm);
	assert(ret == 0);
	char *errmsg;
	fn = ubpf_compile(vm, &errmsg);
	if (fn == NULL) {
		printf("Failed to JIT eBPF program!\n%s\n", errmsg);
		free(errmsg);
		assert(0);
	}
	/* printf("Load eBPF function complete\n"); */
#endif

#ifdef RECVFROM
	libc_recvfrom = bind_symbol("recvfrom");

	/* Setup shared channel */
	struct channel_attr ch_attr = {
		.name = channel_name,
		.ring_size = 512,
	};
	ret = connect_shared_channel(&ch_attr, &vc);
	if (ret) {
		printf("Failed to connect to shared channel\n");
		assert(0);
	}
	/* printf("Connect to shared channel complete\n"); */
#endif
	/* printf("Bind sybmols complete\n"); */
}

static inline void ensure_init(void)
{
	static volatile uint32_t init_cnt = 0;
	static volatile uint8_t init_done = 0;
	static __thread uint8_t in_init = 0;

	if (init_done == 0) {
		if (in_init) {
			return;
		}

		if (__sync_fetch_and_add(&init_cnt, 1) == 0) {
			in_init = 1;
			init();
			in_init = 0;
			MEM_BARRIER();
			init_done = 1;
		} else {
			while (init_done == 0) {
				pthread_yield();
			}
			MEM_BARRIER();
		}
	}
}

