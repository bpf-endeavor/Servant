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
#include <sys/epoll.h>
#include <netinet/in.h>

#include <ubpf.h>

#include "../map.h"
#include "../brain.h"
#include "../interpose_link.h"
// #include "../vchannel.h"

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
/* static char channel_name[] = "rx_data_inject"; */
static struct vchannel vc;
#define MAX_EPOLL_FDS 16
#define MAX_EPOLL_DATA 8

struct _my_ep_data{
	int fd;
	epoll_data_t data;
};

struct my_epoll_data {
	int epfd;
	struct _my_ep_data following [MAX_EPOLL_DATA];
};
static struct my_epoll_data epdata[MAX_EPOLL_FDS] = {};

void _print_socket(int fd)
{
	int ret;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	ret = getsockname(fd, &addr, &addrlen);
	if (!ret) {
		printf("*\t%d: port %d ip: %d (%d)\n", fd,
				ntohs(addr.sin_port),
				ntohl(addr.sin_addr.s_addr), ret);
	}
}

static inline int
_get_epoll_index(int epfd)
{
	int min_empty = -1;
	for (int i = 0; i < MAX_EPOLL_FDS; i++) {
		/* if (epdata[i].epfd > 0) { */
		/* 	for (int j = 0; j < MAX_EPOLL_DATA; j++) { */
		/* 		int fd = epdata[i].following[j].fd; */
		/* 		if (fd) { */
		/* 			ret = getsockname(fd, &addr, &addrlen); */
		/* 			if (!ret) { */
		/* 				printf("*\t%d-%d: port %d ip: %d (%d)\n", epdata[i].epfd, fd, */
		/* 						ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr), ret); */
		/* 			} */
		/* 		} */
		/* 	} */
		/* } */

		if (epdata[i].epfd == epfd) {
			/* printf("\n"); */
			return i;
		} else if(epdata[i].epfd == 0
				&& (i < min_empty || min_empty < 0)) {
			min_empty = i;
		}
	}
	/* printf("\n"); */
	return min_empty;
}

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


/* static int (*libc_epoll_create1)(int flags) = NULL; */
/* int epoll_create1(int flags) */
/* { */
/* 	ensure_init(); */
/* 	return libc_epoll_create1(flags); */
/* } */

/* int epoll_create(int size) */
/* { */
/* 	return epoll_create1(0); */
/* } */

static int (*libc_epoll_ctl)(int epfd, int op, int fd,
    struct epoll_event *event) = NULL;
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	ensure_init();
	int ep_index = _get_epoll_index(epfd);
	if (op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD) {
		if (ep_index >= 0) {
			struct my_epoll_data *ed = (epdata + ep_index);
			if (ed->epfd == 0) {
				// It is a new descriptor
				ed->epfd = epfd;
				ed->following[0].fd = fd;
				ed->following[0].data = event->data;
			} else {
				// Descriptor found
				for (int findex = 0;findex < MAX_EPOLL_DATA; findex++) {
					if (ed->following[findex].fd == 0) {
						// A free data slot found
						ed->following[findex].fd = fd;
						ed->following[findex].data = event->data;
						break;
					}
				}
			}
		}
	} else if (op == EPOLL_CTL_DEL) {
		if (ep_index > 0) {
			// Invalidate
			epdata[ep_index].epfd = 0;
			for (int i = 0; i < MAX_EPOLL_DATA; i++) {
				epdata[ep_index].following[i].fd = 0;
			}
		}
	}
	return libc_epoll_ctl(epfd, op, fd, event);
}

static int (*libc_epoll_wait)(int epfd, struct epoll_event *events, int
		maxevents, int timeout) = NULL;

int epoll_wait(int epfd, struct epoll_event *events, int maxevents,
		int timeout)
{
	ensure_init();
	// expecting at least one buffer to work
	if (maxevents < 1) {
		/* printf("using linux (1)\n"); */
		return libc_epoll_wait(epfd, events, maxevents, timeout);
	}
	int epfd_index = _get_epoll_index(epfd);
	if (epfd_index < 0 || epdata[epfd_index].epfd != epfd) {
		// do not know this epfd
		/* printf("using linux (2) selected index: %d index fd: %d looking for: %d\n", epfd_index, epdata[epfd_index].epfd, epfd); */
		return libc_epoll_wait(epfd, events, maxevents, timeout);
	}
	struct _my_ep_data data = {};
	// TODO: starting from one to bypass listenning socket
	for (int i = 1; i < MAX_EPOLL_DATA; i++) {
		if (epdata[epfd_index].following[i].fd) {
			data = epdata[epfd_index].following[i];
			break;
		}
	}
	if (!data.fd) {
		// don't know the data
		/* printf("using linux (3)\n"); */
		return libc_epoll_wait(epfd, events, maxevents, timeout);
	}

	/* printf("checking both links\n"); */
	const int has_timeout = timeout > -1;
	const int to = has_timeout ? timeout : 20; // ms
	int ret = 0;
	while (ret == 0) {
		ret = vc_count_msg(&vc);
		if (ret) {
			/* _print_socket(data.fd); */
			// There are some messages from runtime.
			/* printf("fd: %d, data: %d", data.fd, data.data.u32); */
			events[0].events = EPOLLIN;
			events[0].data = data.data;
			return 1;
		} else {
			ret = libc_epoll_wait(epfd, events, maxevents, to);
			if (has_timeout)
				break;
		}
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
	/* void *handle; */
	/* if ((handle = dlopen("libc.so.6", RTLD_LAZY)) == NULL) { */
	/* 	perror("tas libc lookup: dlopen on libc failed"); */
	/* 	abort(); */
	/* } */

	libc_recvfrom = bind_symbol("recvfrom");
	/* libc_epoll_create1 = bind_symbol(handle, "epoll_create1"); */
	/* libc_epoll_create1 = bind_symbol("epoll_create1"); */
	libc_epoll_ctl = bind_symbol("epoll_ctl");
	libc_epoll_wait = bind_symbol("epoll_wait");

	/* Setup shared channel */
	ret = setup_interpose_vchannel(&vc);
	/* struct channel_attr ch_attr = { */
	/* 	.name = channel_name, */
	/* 	.ring_size = 512, */
	/* }; */
	/* ret = connect_shared_channel(&ch_attr, &vc); */
	if (ret) {
		printf("Failed to connect to shared channel\n");
		assert(0);
	}
	/* printf("Connect to shared channel complete\n"); */
#endif
	printf("Initializing interpose finished\n");
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

