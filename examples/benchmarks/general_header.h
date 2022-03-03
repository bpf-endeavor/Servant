#ifndef GENERAL_HEADER
#define GENERAL_HEADER

#include <stdbool.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <linux/if_ether.h>

#define out_of_pkt(ptr, end) ((void *)(ptr + 1) > end)

#define sinline static inline __attribute__((__always_inline__))


/* #define ISXDP */
#define ISUBPF

#ifdef ISXDP
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include <string.h>
#define CONTEXT struct xdp_md

#else
#define CONTEXT struct pktctx
#include <servant/servant_engine.h>

#define XDP_DROP 200
#define XDP_TX   300

#define ADJUST_HEAD_DECREASE(xdp, decsize) { \
	xdp->trim_head += decsize; \
	xdp->data += decsize; \
	xdp->pkt_len -= decsize;}

#define ADJUST_HEAD_INCREASE(xdp, incsize) {\
	xdp->trim_head -= incsize; \
	xdp->data -= incsize; \
	xdp->pkt_len += incsize;}

/* #define ADJUST_HEAD_INCREASE(xdp, incsize) {\ */
/* 	int pktsize = xdp->data_end - xdp->data; \ */
/* 	ubpf_memmove(xdp->data + incsize, xdp->data, pktsize); \ */
/* 	xdp->data_end += incsize; \ */
/* 	xdp->pkt_len += incsize;} */

#define bpf_ntohs(x) ubpf_ntohs(x)
#define bpf_htons(x) ubpf_htons(x)

#define SEC(x) 

#endif

#endif