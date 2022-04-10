#ifndef _MEMCPY_BENCH_H
#define _MEMCPY_BENCH_H
#include "../include/packet_context.h"

int memcpy_bpf_prog(struct pktctx *ctx);
int pktproc_bpf_prog(struct pktctx *ctx);
#endif
