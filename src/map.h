#ifndef MAP_H
#define MAP_H

#include "labels.h"


#define MAX_NR_MAPS  10

/** map_names and map_fds are used for identifying the file descriptor of the
 * map that should be accessed during a lookup or an update. They are
 * initialized by setup_map_system function.
 */
extern char *map_names[MAX_NR_MAPS];
extern int map_fds[MAX_NR_MAPS];

/**
 * Tries to implement the bpf_map_lookup_elem semantic. in uBPF environment.
 *
 * @param map_name Name of the map being used (in XDP programs it is the
 * refrence to the map).
 * @param key_ptr A ptr to the key object (same as XDP program).
 * @param buffer  A ptr to the buffer into which result will be copied.
 * @return Returns zero on success
 */
int ubpf_map_lookup_elem(char *map_name, const void *key_ptr, OUT void
		*buffer);

/**
 * The XDP program should be bind to the interface first.
 *
 * This function should be called only once, map data will be overriten.
 *
 * @param ifindex Intreface index
 * @return Returns zero on success.
 */
int setup_map_system(int ifindex);

#endif
