#ifndef MAP_H
#define MAP_H

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
