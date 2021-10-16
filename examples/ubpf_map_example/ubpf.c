#include <stdint.h>

static int (*lookup)(char *name, const void *key, void *val) = (void *)1;

uint32_t bpf_prog(void *argv)
{
	const int zero = 0;
	uint32_t value;
	char map_name[32] = "test_map";
	int ret = lookup((char *)(&map_name[0]), &zero, &value);
	if (!ret) {
		return value;
	} else {
		return 1111;
	}
}
