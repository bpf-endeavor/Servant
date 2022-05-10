#ifndef MAP_H
#define MAP_H

#include "labels.h"


#define MAX_NR_MAPS  16

/** map_names and map_fds are used for identifying the file descriptor of the
 * map that should be accessed during a lookup or an update. They are
 * initialized by setup_map_system function.
 */
/* extern char *map_names[MAX_NR_MAPS]; */
/* extern int map_fds[MAX_NR_MAPS]; */
/* extern size_t map_value_size[MAX_NR_MAPS]; */
/* extern void *map_value_pool[MAX_NR_MAPS]; */

/**
 * Tries to implement the bpf_map_lookup_elem semantic. in uBPF environment.
 *
 * @param map_name Name of the map being used (in XDP programs it is the
 * refrence to the map).
 * @param key_ptr A pointer to the key object (same as XDP program).
 * @return Returns pointer to the value. In case of failure returns NULL.
 */
void *ubpf_map_lookup_elem_kern(char *map_name, const void *key_ptr);

/**
 * Free the memory allocated for map lookup result.
 * @param ptr pointer to map elem to be freed
 */
void ubpf_map_elem_release(void *ptr);

/**
 * @param map_name Name of the map being used (in XDP programs it is the
 * refrence to the map).
 * @param key_ptr A pointer to the key object (same as XDP program).
 * @param value A pointer to value object.
 * @param flag Flags used for update operation.
 * @return Returns zero on success
 */
int ubpf_map_update_elem_kern(char *map_name, const void *key_ptr, void *value,
		int flag);

/**
 * Setup map system by looking for the maps having the name given as argument.
 * @param names An array of MAP names.
 * @param size Size of names array. Maximum array size can be MAX_NR_MAPS.
 * @return Returns zero on success
 */
int setup_map_system(char *names[], int size);

/**
 * Gathers the information of maps from the XDP program that is currently
 * attached to the interface.
 *
 * The XDP program should be bind to the interface first.
 * This function should be called only once, map data will be overriten.
 *
 * @param ifindex Intreface index
 * @return Returns zero on success.
 */
DEPRECATED int setup_map_system_from_if_xdp(int ifindex);


void * ubpf_map_lookup_elem_kern_fast(int index, const void *key_ptr);

#endif
