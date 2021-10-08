#ifndef ARGS_H
#define ARGS_H

#include <errno.h>
#include <getopt.h>
#include <net/if.h> // if_nametoindex

struct config {
    int terminate;
    // Ring Configuration
    uint32_t num_frames;
    uint32_t frame_size;
    uint32_t frame_shift;
    uint32_t headroom;
    uint32_t rx_size;
    uint32_t tx_size;
    // Interface Identifier
    uint32_t ifindex;
    char *ifname;
    uint32_t qid;
    // AF_XDP Configuration
    int busy_poll;
    uint32_t busy_poll_duration;
    uint32_t batch_size;
    int copy_mode;
    // Custom Kernel
    int custom_kern_prog;
    char *custom_kern_path;
    // Dummy
    int args[1]; // it is weired
    uint32_t tmp;
};

extern struct config config = {};

void usage(char *prog_name);
void parse_args(int argc, char *argv[]);

#endif
