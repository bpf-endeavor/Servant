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


#ifndef BPF_F_MMAPABLE
#define BPF_F_MMAPABLE (1U << 10)
#endif

/* hash map */
/* #include "../deps/c-hashmap/map.h" */

/* hashmap* map_names_hash; */
/* fn() { */
/* int name_len = strlen(map_name); */
/* uintptr_t index; */
/* int res = hashmap_get(map_names_hash, map_name, name_len, &index); */
/* if (res) { */
/* 	*idx = index; */
/* 	return map_fds[(int)index]; */
/* } */
/* return 0; */
/* } */
/* ------- */


// TODO (Farbod): Should I use a hash map data structure?
static char *map_names[MAX_NR_MAPS] = {};
static int map_fds[MAX_NR_MAPS] = {};
static size_t map_value_size[MAX_NR_MAPS] = {};
static void *map_value_pool[MAX_NR_MAPS] = {};
static void *mmap_area[MAX_NR_MAPS] = {};

static size_t roundup_page(size_t sz)
{
	size_t page_size = sysconf(_SC_PAGE_SIZE);
	return ((sz + page_size - 1) / page_size) * page_size;
}

/**
 * This function parses mapName:reqIndex pairs
 * @param req: pointer to string to be parsed
 * @param out_name
 * @param out_index
 * */
static void get_name_index_requested(char *req, char **out_name, int *out_index)
{
	char *stringp = req;
	strsep(&stringp, ":");
	if (stringp == NULL) {
		// no request position
		*out_name = strdup(req);
		*out_index = -1;
	} else {
		*out_name = req;
		*out_index = atoi(stringp);
	}
}

int
setup_map_system(char *requests[], int size)
{
	if (size > MAX_NR_MAPS) {
		ERROR("Number of requested maps exceed the limit\n");
		return 1;
	}

	char *names[MAX_NR_MAPS] = {};
	int req_index[MAX_NR_MAPS] ={};
	for (int i = 0; i < size; i++) {
		get_name_index_requested(requests[i], &names[i], &req_index[i]);
	}

	uint32_t id = 0;
	int ret = 0;
	struct bpf_map_info map_info = {};
	uint32_t info_size = sizeof(map_info);
	uint32_t lastGlobalIndex = 0;
	/* Go through all the eBPF maps on the system */
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
		/* Compare the found map's name with our list of names */
		for (int i = 0; i < size; i++) {
			if (!strcmp(names[i], map_info.name)) {
				while (map_fds[lastGlobalIndex] != 0) {
					/* Skip indexes that was used before */
					lastGlobalIndex++;
				}
				int cur_index = -1;
				if (req_index[i] == -1) {
					/* No specific index was requested */
					cur_index = lastGlobalIndex;
				} else {
					cur_index = req_index[i];
					if (map_fds[cur_index] != 0) {
						DEBUG("Map index relocation %d -> %d\n", cur_index, lastGlobalIndex);
						/* if that index was used by others then copy it to new location  in order to fulfill the requested index*/
						/* expecting lastGlobalIndex to be an empty position */
						map_fds[lastGlobalIndex] = map_fds[cur_index];
						map_names[lastGlobalIndex] = map_names[cur_index];
						map_value_size[lastGlobalIndex] = map_value_size[cur_index];
						map_value_pool[lastGlobalIndex] = map_value_pool[cur_index];
						mmap_area[lastGlobalIndex] = mmap_area[cur_index];
					}
				}

				map_fds[cur_index] = map_fd;
				map_names[cur_index] = names[i];
				map_value_size[cur_index] = map_info.value_size;

				void *buffer = malloc(map_value_size[cur_index]);
				if (!buffer) {
					ERROR("Failed to allocate map value pool object\n");
					return 1;
				}
				map_value_pool[cur_index] = buffer;

				INFO("* map id: %ld map name: %s (internal index: %d fd: %d)\n", id, map_info.name, cur_index, map_fd);
				if (map_info.map_flags & BPF_F_MMAPABLE) {
					const size_t map_sz = roundup_page((size_t)map_info.value_size * map_info.max_entries);
					INFO("# map name: %s is mmapped\n", map_info.name);
					INFO("# details (size: %ld fd: %d value size: %d entries: %d)\n",
							map_sz, map_fd, map_value_size[cur_index], map_info.max_entries);
					void *m = mmap(NULL, map_sz, PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0);
					if (m == MAP_FAILED) {
						ERROR("Failed to memory map 'ebpf MAP' size: %ld\n", map_sz);
						mmap_area[cur_index] = NULL;
						/* return 1; */
					} else {
						mmap_area[cur_index] = m;
					}
				} else {
					mmap_area[cur_index] = NULL;
				}
			}
		}
	}

	/* DEBUG("Memory map addresses:\n"); */
	/* for (int i = 0; i <  10; i++) { */
	/* 	DEBUG("fd:%d -> mmap: %p\n", map_fds[i], mmap_area[i]); */
	/* } */
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

inline void *
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
		/* free(buffer); */
		return NULL;
	}
	return buffer;
}

void *
ubpf_map_lookup_elem_kern_fast(int index, const void *key_ptr)
{
	if (mmap_area[index] != NULL) {
		/* DEBUG("mmap access %d %d\n", index, *(uint32_t *)key_ptr); */
		// if memory mapped then key is integer (?!)
		return mmap_area[index] + (map_value_size[index] * (*(uint32_t *)key_ptr));
	}
	void *buffer = map_value_pool[index];
	if (!buffer) {
		ERROR("Failed to allocate\n");
		return NULL;
	}
	// copies value form kernel to the buffer
	int ret = bpf_map_lookup_elem(map_fds[index], key_ptr, buffer);
	if (ret != 0) {
		/* DEBUG("Item not found\n"); */
		/* free(buffer); */
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

