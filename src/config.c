#include <math.h>
#include "config.h"

void usage(char *prog_name)
{
    const char desc[] = ("Usage: %s [--Options] <ifname> <qid>\n"
            "*\t ifname: name of interface to attach to\n"
            "*\t qid:    number of queue to attach to\n"
            "Options:\n"
            "\t num-frames, frame-size, batch-size,rx-size, tx-size,\n"
            "\t copy-mode, rps\n"
            );
    printf(desc, prog_name);
}
 

void parse_args(int argc, char *argv[], struct config *config)
{
    // Default Values
    config->frame_size = 4096;
    config->frame_shift = log2(config->frame_size);
    config->headroom = 0;
    config->busy_poll = 0;
    config->busy_poll_duration = 50;
    config->batch_size = 64;
    config->benchmark_done = 0;
    config->rx_size = 2048;
    config->tx_size = 1024;
    config->copy_mode = XDP_ZEROCOPY;
    // config->copy_mode = XDP_COPY;
    config->custom_kern_prog = 0;
    config->custom_kern_path = NULL;

    enum opts {
        NUM_FRAMES = 100,
        FRAME_SIZE,
        BATCH_SIZE,
        RX_SIZE,
        TX_SIZE,
        COPY_MODE,
    };
    struct option long_opts[] = {
        {"num-frames", required_argument, NULL, NUM_FRAMES},
        {"frame-size", required_argument, NULL, FRAME_SIZE},
        {"batch-size", required_argument, NULL, BATCH_SIZE},
        {"rx-size", required_argument, NULL, RX_SIZE},
        {"tx-size", required_argument, NULL, TX_SIZE},
        {"copy-mode", no_argument, NULL, COPY_MODE},
    };
    int ret;
    char optstring[] = "";
    while (1) {
        ret = getopt_long(argc, argv, optstring, long_opts, NULL);
        if (ret == -1)
            break;
        switch (ret) {
            case NUM_FRAMES:
                // config->num_frames = atoi(optarg);
                msg(INFO, "Number of frames is determined automatically\n");
                break;
            case FRAME_SIZE:
                config->frame_size = atoi(optarg);
                config->frame_shift = log2(config->frame_size);
                break;
            case BATCH_SIZE:
                config->batch_size = atoi(optarg);
                break;
            case RX_SIZE:
                config->rx_size = atoi(optarg);
                break;
            case TX_SIZE:
                config->tx_size = atoi(optarg);
                break;
            case COPY_MODE:
                config->copy_mode = XDP_COPY;
                break;
            default:
                msg(ERROR, "Unknown: argument!\n");
                exit(EXIT_FAILURE);
                break;
        }
    }
    if (argc - optind < 2) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    config->ifname = argv[optind];
    config->ifindex = if_nametoindex(config->ifname);
    if(config->ifindex < 0) {
        msg(ERROR, "interface %s not found (%d)\n",
                config->ifname, config->ifindex);
        exit(EXIT_FAILURE);
    }
    optind++;
    config->qid = atoi(argv[optind]);
    optind++;

    // How many descriptors are needed
    config->num_frames = (config->rx_size + config->tx_size) * 4;

    if(config->busy_poll){
        printf("BUSY POLLING\n");
    }
    if (config->copy_mode == XDP_ZEROCOPY) {
        printf("Running in ZEROCOPY mode!\n");
    } else if (config->copy_mode == XDP_COPY) {
        printf("Running in COPY mode!\n");
    } else {
        fprintf(stderr, "Copy mode was not detected!\n");
        exit(EXIT_FAILURE);
    }
    printf("Batch Size: %d\n", config->batch_size);
    printf("Rx Ring Size: %d\n", config->rx_size);
    printf("Tx Ring Size: %d\n", config->tx_size);
}

