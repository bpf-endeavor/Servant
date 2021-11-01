#ifndef SERVANT_ENGINE_H
#define SERVANT_ENGINE_H

// Return values
#define PASS 100
#define DROP 200
#define SEND 300

#ifndef memcpy
# define memcpy(dest, src, n)   __builtin_memcpy((dest), (src), (n))
#endif

// Map access functions
static void *(*lookup)(char *name, const void *key) = (void *)1;
static int (*free_elem)(void *ptr) = (void *)3;
static void (*ubpf_print)(char *fmt, ...) = (void *)4;

// This macro helps with printing things from uBPF
#define DUMP(x, args...) { char fmt[] = x; ubpf_print((char *)fmt, ##args); } 

// Context parameter type
struct pktctx {
	void *data;
	void *data_end;
	size_t pkt_len;
};

#endif
