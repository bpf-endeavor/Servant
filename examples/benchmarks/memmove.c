#include "general_header.h"

#define REPEAT 4


__attribute__((__always_inline__))
static inline int init(char *src, int size) {
	for (int i = 0; i < size; i++) {
		src[i] = (i * i) & 0xff;
	}
	return 0;
}

__attribute__((__always_inline__))
static inline int cpy(char *dst, char *src, unsigned int size)
{
	unsigned int off = 0;
	for (;off + sizeof(unsigned long) <= size ;) {
		if (off > 210) break;
		*((unsigned long *) &dst[off]) = *((unsigned long *) &src[off]);
		off += sizeof(unsigned long);
	}
	for (; off < size; off++) {
		if (off > 210) break;
		dst[off] = src[off];
	}
	return 0;
}

#ifdef ISUBPF
int bpf_porg(CONTEXT *ctx) {
	void *data = (void *)(long)ctx->data;
	char dst[200] = {};
	unsigned short len = ctx->data_end - ctx->data;
	for (int i = 0; i < REPEAT; i++) {
		init(dst, 200);
		/* cpy(dst, data, 200); */
		ubpf_memmove(dst, data, 200);
	}
	/* cpy(data, dst, len); */
	ubpf_memmove(data, dst, len);
	return 0;
}
#endif
