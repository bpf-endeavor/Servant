#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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

#define MEM_BARRIER() __asm__ volatile("" ::: "memory")

#define UBPF_PROG_PATH "/users/farbod/BMC/bmc/uth.o"

/* The address of jitted function is set here */
static ubpf_jit_fn fn;

static inline void ensure_init(void);

static ssize_t (*libc_sendmsg)(int sockfd, const struct msghdr *msg,
		int flags);

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	ensure_init();
	/* printf("hello from userspace tx hook\n"); */
	/* Run eBPF function */
	/* sendmsg promises that it wont change msg, but I might do! */
	fn((struct msghdr *)msg, sizeof(struct msghdr));
	return libc_sendmsg(sockfd, msg, flags);
}

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
	libc_sendmsg = bind_symbol("sendmsg");
	/* Setup map system */
	char *names[] = {
		"map_kcache",
		"map_stats",
	};
	const int count = 2;
	setup_map_system(names, count);
	struct ubpf_vm *vm;
	int ret = setup_ubpf_engine(UBPF_PROG_PATH, &vm);
	assert(ret == 0);
	char *errmsg;
	fn = ubpf_compile(vm, &errmsg);
	if (fn == NULL) {
		printf("Failed to JIT eBPF program!\n%s\n", errmsg);
		free(errmsg);
		assert(0);
	}
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

