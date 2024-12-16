#include "general_header.h"

#ifdef ISUBPF
int bpf_prog(CONTEXT *ctx)
{
	return YIELD;
}

int bpf_prog2(CONTEXT *ctx)
{
	return DROP;
}
#else
SEC("prog")
int bpf_prog(CONTEXT *ctx) {
	return XDP_DROP;
}
#endif

char _license[] SEC("license") = "GPL";
