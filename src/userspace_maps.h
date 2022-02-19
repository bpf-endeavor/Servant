#ifndef USERSPACE_MAPS_CTL_H
#define USERSPACE_MAPS_CTL_H
#include <ubpf.h>

struct server_conf {
	/* Indicates ifS the server should be listening */
	int running;
	/* VM to use for accessing the maps */
	struct ubpf_vm *vm;
};

enum CommandIDs {
	READ = 100,
	WRITE,
	LIST,
	EXIT = 300,
};

struct cmd {
	enum CommandIDs cmdid;
	int key;
	int value;
};

int launch_userspace_maps_server(struct server_conf *config);
#endif
