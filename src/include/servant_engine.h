#ifndef SERVANT_ENGINE_H
#define SERVANT_ENGINE_H

#ifndef memcpy
# define memcpy(dest, src, n)   __builtin_memcpy((dest), (src), (n))
#endif

// Map access functions
static void *(*lookup)(char *name, const void *key) = (void *)1;
static int (*free_elem)(void *ptr) = (void *)3;
static void (*ubpf_print)(char *fmt, ...) = (void *)4;

// This macro helps with printing things from uBPF
#define DUMP(x, args...) { char fmt[] = x; ubpf_print((char *)fmt, ##args); } 

#include "packet_context.h"
#endif
