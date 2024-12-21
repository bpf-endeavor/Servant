#ifndef SERVANT_ENGINE_H
#define SERVANT_ENGINE_H

#ifndef memcpy
# define memcpy(dest, src, n)   __builtin_memcpy((dest), (src), (n))
#endif

#ifndef memmove
# define memmove(dest, src, n)  __builtin_memmove((dest), (src), (n))
#endif

#ifndef memset
# define memset(dest, chr, n)   __builtin_memset((dest), (chr), (n))
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
#define ubpf_htonl(x)		__builtin_bswap32(x)
#define ubpf_ntohl(x)		__builtin_bswap32(x)
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
		__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ubpf_ntohs(x)		(x)
#define ubpf_htons(x)		(x)
#define ubpf_ntohl(x)		(x)
#define ubpf_htonl(x)		(x)
#else
# error "Endianness detection needs to be set up for your compiler?!"
#endif

// In kernel map access functions
static void *(*lookup)(char *name, const void *key) = (void *)1;
static void *(*lookup_fast)(int index, const void *key_ptr) = (void *)2;
/* For debugging */
static void (*ubpf_print)(char *fmt, ...) = (void *)3;
/* For reading timestamp CPU */
static unsigned long int (*ubpf_rdtsc)(void) = (void *)4;
/* memmove */
static void (*ubpf_memmove)(void *d, void *s, uint32_t n) = (void *)5;
/* Userspace Maps */
static void *(*userspace_lookup)(const void *, const void *) = (void *)6;
static int (*userspace_update)(void *, const void *, void *) = (void *)7;
/* Get time in nanosecond */
static uint64_t (*ubpf_time_get_ns)(void) = (void *)8;
/* Prototyping splitting */
static int (*userspace_lookup_p1)(const void *, const void *) = (void *)9;
static void *(*userspace_lookup_p2)(const void *, const void *) = (void *)10;

/* Should match the numbers in uBPF VM */
static void (*ubpf_prefetch)(void *) = (void *)128;

// This macro helps with printing things from uBPF
#define DUMP(x, args...) { char fmt[] = x; ubpf_print((char *)fmt, ##args); } 

#define DUMP_PKT_HEX(xdp) { \
	char fmt1[] = "%02x "; \
	char fmt2[] = "\n"; \
	uint8_t *c  = xdp->data; \
	for (int i = 0; i < xdp->pkt_len; i++) { \
		ubpf_print((char *)fmt1, c[i]); \
		if ((i + 1) % 16 == 0) { \
			ubpf_print((char *)fmt2); \
		} \
	} \
	ubpf_print((char *)fmt2); }

#ifndef NULL
#define NULL 0
#endif

#include "packet_context.h"
#include <ubpf_maps.h>
#endif
