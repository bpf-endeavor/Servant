#ifndef SERVANT_ENGINE_H
#define SERVANT_ENGINE_H

#ifndef memcpy
# define memcpy(dest, src, n)   __builtin_memcpy((dest), (src), (n))
#endif

#ifndef _STDINT_H
typedef unsigned long int uint64_t;
typedef unsigned int      uint32_t;
typedef unsigned short    uint16_t;
typedef unsigned char     uint8_t;
#ifndef _BITS_STDINT_INTN_H
typedef long int          int64_t;
typedef int               int32_t;
typedef short             int16_t;
typedef char              int8_t;
#endif
#endif

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
		__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ubpf_ntohs(x)		__builtin_bswap16(x)
#define ubpf_htons(x)		__builtin_bswap16(x)
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
		__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ubpf_ntohs(x)		(x)
#define ubpf_htons(x)		(x)
#else
# error "Endianness detection needs to be set up for your compiler?!"
#endif

// Map access functions
static void *(*lookup)(char *name, const void *key) = (void *)1;
static int (*free_elem)(void *ptr) = (void *)3;
static void (*ubpf_print)(char *fmt, ...) = (void *)4;
static unsigned long int (*ubpf_rdtsc)(void) = (void *)5;

// This macro helps with printing things from uBPF
#define DUMP(x, args...) { char fmt[] = x; ubpf_print((char *)fmt, ##args); } 

#ifndef NULL
#define NULL 0
#endif


#include "packet_context.h"
#endif
