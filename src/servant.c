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


#include <pthread.h>

#include "sockets.h"
#include "config.h" // application configuration and parsing arguments
#include "defs.h"   // af_xdp structs
#include "log.h"
#include "heart.h"


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
    parse_args(argc, argv);
    setRlimit();

    struct xsk_socket_info *xsk = setup_socket(config.ifname, config.qid);

    // Add interrupt handler
    signal(SIGINT,  int_exit);
    signal(SIGTERM, int_exit);
    signal(SIGABRT, int_exit);

    // Start report thread
    /* pthread_t report_thread; */
    /* pthread_create(&report_thread, NULL, report, (void *)xsk); */

    // Poll the socket and send receive packets to eBPF engine
    pump_packets(xsk);

    // Clean up
    /* pthread_join(report_thread, NULL); */
    tear_down_socket(xsk);
    INFO("Done!\n");
    return 0;
}

