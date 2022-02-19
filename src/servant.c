/**
 * This is the main file of servant runtime system
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <signal.h>

#include <linux/if_xdp.h>
#include <linux/if_link.h>

#include <bpf/libbpf.h>
#include <bpf/bpf.h>


/* #include <pthread.h> */

#include "sockets.h"
#include "map.h"
#include "config.h" // application configuration and parsing arguments
#include "defs.h"   // af_xdp structs
#include "log.h"
#include "heart.h"
#include "brain.h"
#include "interpose_link.h"
#include "userspace_maps.h"


static void int_exit() {
	config.terminate = 1;
}

static void setRlimit()
{
	struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
	if (setrlimit(RLIMIT_MEMLOCK, &r)) {
		ERROR("setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	int ret;
	parse_args(argc, argv);
	setRlimit();

	// If needed load custom XDP prog
	if (config.custom_kern_prog && config.custom_kern_path[0] != '-') {
		load_xdp_program(config.custom_kern_path, config.ifindex);
	}
	struct xsk_socket_info *xsk = setup_socket(config.ifname, config.qid);
	/* ret = setup_map_system(config.ifindex); */
	char *map_names[] = {
		"xsks_map",
		/* For BMC */
		"map_kcache",
		"map_stats",
		/* For testing */
		"test_map",
		"data_map",
		/* For Katran */
		"ctl_array",
	};
	const int count_map_names = 6;
	ret = setup_map_system(map_names, count_map_names);
	if (ret)
		goto teardown;
	if (config.custom_kern_prog) {
		// enter XSK to the map for receiving traffic
		ret = enter_xsks_into_map(xsk, config.qid);
		if (ret) {
			goto teardown;
		}
	}

	if (config.has_uth) {
		/* Setup Userspace Tx Hook */
		/* ret = uth_setup(); */
		/* if (ret) { */
		/* goto teardown; */
		/* } */
		INFO("UTH has not implemented yet\n");
	}

	if (config.use_packet_injection)
		setup_interpose_link();

	// Add interrupt handler
	signal(SIGINT,  int_exit);
	signal(SIGTERM, int_exit);
	signal(SIGABRT, int_exit);

	// Start report thread
	/* pthread_t report_thread; */
	/* pthread_create(&report_thread, NULL, report, (void *)xsk); */

	// Setup eBPF engine
	struct ubpf_vm *vm;
	ret = setup_ubpf_engine(config.ebpf_program_path, &vm);
	if (ret) {
		goto teardown2;
	}

	/* Launch command server */
	struct server_conf sconf  = {
		.running = 1,
		.vm = vm,
	};
	launch_userspace_maps_server(&sconf);


	// Poll the socket and send receive packets to eBPF engine
	pump_packets(xsk, vm);

	// Clean up
	/* pthread_join(report_thread, NULL); */
	sconf.running = 0;
	ubpf_destroy(vm);
teardown2:
	if (config.use_packet_injection)
		teardown_interpose_link();
teardown:
	if (config.custom_kern_prog && config.custom_kern_path[0] != '-') {
		// Remove XDP program
		int xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST | config.xdp_mode;
		bpf_set_link_xdp_fd(config.ifindex, -1, xdp_flags);
		INFO("Unlinked XDP program\n");
	}
	tear_down_socket(xsk);
	INFO("Done!\n");
	return 0;
}

