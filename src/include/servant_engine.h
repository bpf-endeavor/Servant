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


// Context parameter type

struct pktctx {
	void *data;
	void *data_end;
};

#endif
