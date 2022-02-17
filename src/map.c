#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/if_link.h> // some XDP flags
#include <sys/mman.h> // mmap
#include <unistd.h>  // sysconf
#include <bpf/libbpf.h> // bpf_get_link_xdp_id
#include <bpf/bpf.h> // bpf_prog_get_fd_by_id, bpf_obj_get_info_by_fd, ...

#include "map.h"
/* #include "config.h" */
#include "log.h"


// TODO (Farbod): Should I use a hash map data structure?
char *map_names[MAX_NR_MAPS] = {};
int map_fds[MAX_NR_MAPS] = {};
size_t map_value_size[MAX_NR_MAPS];
void *map_value_pool[MAX_NR_MAPS];
void *mmap_area[MAX_NR_MAPS];

static size_t roundup_page(size_t sz)
{
	size_t page_size = sysconf(_SC_PAGE_SIZE);
	return ((sz + page_size - 1) / page_size) * page_size;
}

int
setup_map_system(char *names[], int size)
{
	if (size > MAX_NR_MAPS) {
		ERROR("Number of requested maps exceed the limit\n");
		return 1;
	}
	uint32_t id = 0;
	int ret = 0;
	struct bpf_map_info map_info = {};
	uint32_t info_size = sizeof(map_info);
	uint32_t lastGlobalIndex = 0;
	while (!ret) {
		ret = bpf_map_get_next_id(id, &id);
		if (ret) {
			if (errno == ENOENT)
				break;
			ERROR("can't get next map: %s%s", strerror(errno),
				errno == EINVAL ? " -- kernel too old?" : "");
			break;
		}
		int map_fd = bpf_map_get_fd_by_id(id);
		bpf_obj_get_info_by_fd(map_fd, &map_info, &info_size);
		for (int i = 0; i < size; i++) {
			if (!strcmp(names[i], map_info.name)) {
				INFO("* map id: %ld map name: %s\n", id, map_info.name);
				map_fds[lastGlobalIndex] = map_fd;
				map_names[lastGlobalIndex] = strdup(map_info.name);
				map_value_size[lastGlobalIndex] = map_info.value_size;

				void *buffer = malloc(map_value_size[lastGlobalIndex]);
				if (!buffer) {
					ERROR("Failed to allocate map value pool object\n");
					return 1;
				}
				map_value_pool[lastGlobalIndex] = buffer;

				if (map_info.map_flags & BPF_F_MMAPABLE) {
					const size_t map_sz = roundup_page((size_t)map_info.value_size * map_info.max_entries);
					INFO("# map name: %s is mmapped\n", map_info.name);
					INFO("# details (size: %ld fd: %d value size: %d entries: %d)\n",
							map_sz, map_fd, map_value_size[lastGlobalIndex], map_info.max_entries);
					void *m = mmap(NULL, map_sz, PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0);
					if (m == MAP_FAILED) {
						ERROR("Failed to memory map 'ebpf MAP' size: %ld\n", map_sz);
						mmap_area[lastGlobalIndex] = NULL;
						/* return 1; */
					} else {
						mmap_area[lastGlobalIndex] = m;
					}
				} else {
					mmap_area[lastGlobalIndex] = NULL;
				}

				lastGlobalIndex++;
			}
		}
	}
	// Fail just for testing
	return 0;
}

/* int */
/* setup_map_system_from_if_xdp(int ifindex) */
/* { */
/* 	// Get to XDP program Id connected to the interface */
/* 	int xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST | config.xdp_mode; */
/* 	uint32_t prog_id; */
/* 	int ret; */
/* 	ret = bpf_get_link_xdp_id(ifindex, &prog_id, xdp_flags); */
/* 	if (ret < 0) { */
/* 		ERROR("Failed to get link program id\n"); */
/* 	} else { */
/* 		if (!prog_id) { */
/* 			INFO("Setup Map System: No XDP program found!\n", */
/* 					prog_id); */
/* 			return 1; */
/* 		} */
/* 	} */

/* 	struct bpf_map_info map_info = {}; */
/* 	struct bpf_prog_info prog_info = { */
/* 		.nr_map_ids = MAX_NR_MAPS, */
/* 		.map_ids = (uint64_t)calloc(MAX_NR_MAPS, sizeof(uint32_t)), */
/* 	}; */
/* 	int prog_fd = bpf_prog_get_fd_by_id(prog_id); */
/* 	uint32_t info_size = sizeof(prog_info); */
/* 	bpf_obj_get_info_by_fd(prog_fd, &prog_info, &info_size); */

/* 	uint32_t count_maps = prog_info.nr_map_ids; */
/* 	INFO("Program %s has %d maps\n", prog_info.name, count_maps); */
/* 	uint32_t *map_ids = (uint32_t *)prog_info.map_ids; */
/* 	info_size = sizeof(map_info); // for passing to bpf_obj_get_info_by_fd */
/* 	for (int i = 0; i < count_maps; i++) { */
/* 		int map_fd = bpf_map_get_fd_by_id(map_ids[i]); */
/* 		bpf_obj_get_info_by_fd(map_fd, &map_info, &info_size); */
/* 		INFO("* %d: map id: %ld map name: %s\n", i, map_ids[i], map_info.name); */
/* 		map_fds[i] = map_fd; */
/* 		map_names[i] = strdup(map_info.name); */
/* 		map_value_size[i] = map_info.value_size; */

/* 		void *buffer = malloc(map_value_size[i]); */
/* 		if (!buffer) { */
/* 			ERROR("Failed to allocate map value pool object\n"); */
/* 			return 1; */
/* 		} */
/* 		map_value_pool[i] = buffer; */
/* 	} */
/* 	return 0; */
/* } */

/**
 * @return Returns non-zero value on success.
 */
static int
_get_map_fd(char *map_name)
{
	for (int i = 0; i < MAX_NR_MAPS; i++) {
		if (map_names[i] == NULL) {
			// List finished and did not found the FD of the map
			return 0;
		} else if (!strcmp(map_names[i], map_name)) {
			// Found the map by its name
			return map_fds[i];
		}
	}
	return 0;
}

static int
_get_map_fd_and_idx(char *map_name, int *idx)
{
	for (int i = 0; i < MAX_NR_MAPS; i++) {
		if (map_names[i] == NULL) {
			// List finished and did not found the FD of the map
			return 0;
		} else if (!strcmp(map_names[i], map_name)) {
			// Found the map by its name
			*idx = i;
			return map_fds[i];
		}
	}
	return 0;
}

void *
ubpf_map_lookup_elem_kern(char *map_name, const void *key_ptr)
{
	int idx;
	int fd = _get_map_fd_and_idx(map_name, &idx);
	if (!fd) {
		ERROR("Failed to find the map %s \n", map_name);
		return NULL;
	}
	if (mmap_area[idx] != NULL) {
		// if memory mapped then key is integer (?!)
		return mmap_area[idx] + (map_value_size[idx] * (*(uint32_t *)key_ptr));
	}
	void *buffer = map_value_pool[idx];
	if (!buffer) {
		ERROR("Failed to allocate\n");
		return NULL;
	}
	// copies value form kernel to the buffer
	int ret = bpf_map_lookup_elem(fd, key_ptr, buffer);
	if (ret != 0) {
		/* DEBUG("Item not found\n"); */
		free(buffer);
		return NULL;
	}
	return buffer;
}

void
ubpf_map_elem_release(void *ptr)
{
	/* DEBUG("Free %p\n", ptr); */
	/* free(ptr); */
}

int
ubpf_map_update_elem_kern(char *map_name, const void *key_ptr, void *value, int flag)
{
	int fd = _get_map_fd(map_name);
	if (!fd)
		return 1;
	return bpf_map_update_elem(fd, key_ptr, value, flag);
}

