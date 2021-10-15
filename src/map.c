#include <stdlib.h>
#include <linux/if_link.h> // some XDP flags
#include <bpf/libbpf.h> // bpf_get_link_xdp_id
#include <bpf/bpf.h> // bpf_prog_get_fd_by_id, bpf_obj_get_info_by_fd, ...

#include "map.h"
#include "config.h"
#include "log.h"


#define MAX_NR_MAPS  10
// TODO (Farbod): Should I use a hash map data structure?
char *map_names[MAX_NR_MAPS];
int map_fds[MAX_NR_MAPS];


int
setup_map_system(int ifindex)
{
	// Get to XDP program Id connected to the interface
	int xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST | config.xdp_mode;
	uint32_t prog_id;
	int ret;
	ret = bpf_get_link_xdp_id(ifindex, &prog_id, xdp_flags);
	if (ret < 0) {
		ERROR("Failed to get link program id\n");
	} else {
		if (prog_id < 0) {
			INFO("Setup Map System: No XDP program found (id: %d)\n", prog_id);
			return 1;
		}
	}

	struct bpf_map_info map_info = {};
	struct bpf_prog_info prog_info = {
		.nr_map_ids = MAX_NR_MAPS,
		.map_ids = (uint64_t)calloc(MAX_NR_MAPS, sizeof(uint32_t)),
	};
	int prog_fd = bpf_prog_get_fd_by_id(prog_id);
	uint32_t info_size = sizeof(prog_info);
	bpf_obj_get_info_by_fd(prog_fd, &prog_info, &info_size);

	uint32_t count_maps = prog_info.nr_map_ids;
	INFO("Program %s has %d maps\n", prog_info.name, count_maps);
	uint32_t *map_ids = (uint32_t *)prog_info.map_ids;
	info_size = sizeof(map_info); // for passing to bpf_obj_get_info_by_fd
	for (int i = 0; i < count_maps; i++) {
		int map_fd = bpf_map_get_fd_by_id(map_ids[i]);
		bpf_obj_get_info_by_fd(map_fd, &map_info, &info_size);
		INFO("* %d: map id: %ld map name: %s\n", i, map_ids[i], map_info.name); 
	}
	return 0;
}

