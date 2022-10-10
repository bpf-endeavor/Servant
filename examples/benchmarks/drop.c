#include "general_header.h"

SEC("prog")
int bpf_prog(CONTEXT *ctx)
{
	return XDP_DROP;
}

char _license[] SEC("license") = "GPL";
