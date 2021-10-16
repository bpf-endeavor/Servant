#include <stdlib.h> // exit
#include <string.h> // strerror
#include <sys/mman.h> // mmap
#include <linux/if_link.h> // some XDP flags

#include "sockets.h"
#include "config.h"
#include "log.h"
#include "map.h"


// TODO (Farbod): xdp_flags might be broken to a constant part that is globally
// defined and a dynamic part in the config. Currently the knowledge is shared
// between the function using it.


int
load_xdp_program(char *xdp_filename, int ifindex)
{
	int xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST | config.xdp_mode;
	uint32_t prog_id;
	int ret;
	ret = bpf_get_link_xdp_id(ifindex, &prog_id, xdp_flags);
	if (ret < 0) {
		ERROR("Failed to get link program id\n");
	} else {
		if (prog_id > 0) {
			INFO("There is already a loaded XDP (id: %d)\n", prog_id);
			// load_maps();
			return 1;
		}
	}
	struct bpf_prog_load_attr prog_load_attr = {
		.prog_type      = BPF_PROG_TYPE_XDP,
		.file           = xdp_filename,
	};
	int prog_fd;
	UNUSED struct bpf_object *obj;
	if (bpf_prog_load_xattr(&prog_load_attr, &obj, &prog_fd)) {
		ERROR("Failed to load custom xdp prog (%s)\n", xdp_filename);
		exit(EXIT_FAILURE);
	}
	if (prog_fd < 0) {
		ERROR("no program found: %s\n", strerror(prog_fd));
		exit(EXIT_FAILURE);
	}
	if (bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags) < 0) {
		ERROR("link set XDP fd failed\n");
		INFO("Warning: probably program is already attached but if not"
				"error handling has not been done!\n");
		// exit(EXIT_FAILURE);
	}
	return 0;
}

struct xsk_socket_info *
setup_socket(char *ifname, uint32_t qid)
{
    struct xsk_umem_info *umem = NULL;
    int ret = 0;
    void *bufs = NULL;
    struct xsk_socket_info *xsk;
    /* struct bpf_object *xdp_obj; */

    // memory for umem
    uint64_t umem_size = config.num_frames * config.frame_size;
    bufs = mmap(NULL, umem_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (bufs == MAP_FAILED) {
        ERROR("mmap failed\n");
        exit(EXIT_FAILURE);
    }
    // creating umem
    const uint32_t fill_size = config.rx_size * 2;
    {
        struct xsk_umem_config cfg = {
            .fill_size = fill_size,
            .comp_size = config.tx_size * 2,
            .frame_size = config.frame_size,
            .frame_headroom = config.headroom,
            .flags = 0
        };
        umem = calloc(1, sizeof(struct xsk_umem_info));
        if (!umem) {
            ERROR("allocating umem!\n");
            exit(EXIT_FAILURE);
        }
        ret = xsk_umem__create(&umem->umem, bufs, umem_size, &umem->fq,
                &umem->cq, &cfg);
        if (ret) {
            ERROR("creating umem!\n");
            exit(EXIT_FAILURE);
        }
        umem->buffer = bufs;
        // Populate Fill Ring
        uint32_t idx = 0;
        ret = xsk_ring_prod__reserve(&umem->fq, fill_size, &idx);
        if (ret != fill_size) {
            ERROR("populate fill ring!\n");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < fill_size; i++) {
            *xsk_ring_prod__fill_addr(&umem->fq, idx++) =
                i * config.frame_size;
        }
        xsk_ring_prod__submit(&umem->fq, fill_size);
    }

    // Create socket
    {
        struct xsk_socket_config cfg;
        xsk = calloc(1, sizeof(struct xsk_socket_info));
        if (!xsk) {
            ERROR("ERROR: allocating socket!\n");
            exit(EXIT_FAILURE);
        }
        xsk->umem = umem;
        cfg.rx_size = config.rx_size;
        cfg.tx_size = config.tx_size;
        if (config.custom_kern_prog) {
            cfg.libbpf_flags = XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD;
        } else {
            cfg.libbpf_flags = 0;
        }
        cfg.xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST | config.xdp_mode;
        cfg.bind_flags = config.copy_mode;
        ret = xsk_socket__create(&xsk->xsk, ifname, qid,
                umem->umem, &xsk->rx, &xsk->tx, &cfg);
        if (ret) {
            ERROR("Creating socket failed! (%s)\n", strerror(-ret));
            ERROR("Mellanox and ZEROCOPY does not work well (need configuration)!\n");
            ERROR("Netronome and ZEROCOPY does not work well!\n");
            exit(EXIT_FAILURE);
        }
    }
    // Apply some socket options
    if(config.busy_poll) {
        int sock_opt;
        sock_opt = 1;
        if (setsockopt(xsk_socket__fd(xsk->xsk), SOL_SOCKET, SO_PREFER_BUSY_POLL,
                    (void *)&sock_opt, sizeof(sock_opt)) < 0)
            exit(EXIT_FAILURE);

        sock_opt = config.busy_poll_duration;
        if (setsockopt(xsk_socket__fd(xsk->xsk), SOL_SOCKET, SO_BUSY_POLL,
                    (void *)&sock_opt, sizeof(sock_opt)) < 0)
            exit(EXIT_FAILURE);

        sock_opt = config.batch_size;
        if (setsockopt(xsk_socket__fd(xsk->xsk), SOL_SOCKET, SO_BUSY_POLL_BUDGET,
                    (void *)&sock_opt, sizeof(sock_opt)) < 0)
            exit(EXIT_FAILURE);
    }
    /* if (config.custom_kern_prog) { */
    /*     enter_xsks_into_map(xdp_obj, xsk, rps_index); */
    /* } */
    return xsk;
}

void
tear_down_socket(struct xsk_socket_info *xsk)
{
    uint64_t umem_size = config.num_frames * config.frame_size;
    xsk_socket__delete(xsk->xsk);
    xsk_umem__delete(xsk->umem->umem);
    munmap(xsk->umem->buffer, umem_size);
    free(xsk->umem);
    free(xsk);
}

int
enter_xsks_into_map( struct xsk_socket_info *xsk, int qid)
{
	if (qid < 0 || qid > MAX_QID)
		return 1;
	int fd = xsk_socket__fd(xsk->xsk);
	int ret = ubpf_map_update_elem("xsks_map", &qid, &fd, 0);
	if (ret) {
		ERROR("ERROR: bpf_map_update_elem %d\n", qid);
		return ret;
	}
	INFO("Add socket to xsks_map index %d\n", qid);
	return 0;
}
