#include <stdint.h>
#include "../../src/include/servant_engine.h"

int bpf_prog(void *argv)
{
	int ret = -1;
	const int zero = 0;
	char map_name[32] = "test_map";
	int *value = lookup((char *)(&map_name[0]), &zero);
	if (value != 0) {
		ret = *value;
		free_elem(value);
	}
	return ret;
}
