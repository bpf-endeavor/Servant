#include <linux/bpf.h>
#include <linux/if_link.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/resource.h>
#include <net/if.h>

/* #include "bpf_util.h" */
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

static int ifindex;
static __u32 xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
static __u32 prog_id;

static void wait(void);
static void usage(const char *prog);
static void int_exit(int sig);

int main(int argc, char **argv)
{
	struct bpf_prog_load_attr prog_load_attr = {
		.prog_type	= BPF_PROG_TYPE_XDP,
	};
	struct bpf_prog_info info = {};
	__u32 info_len = sizeof(info);
	const char *optstr = "FSNp:";
	int prog_fd;
	int opt;
	struct bpf_object *obj;
	struct bpf_program *prog;
	const int max_name_size = 256;
	char prog_sec_name[max_name_size];
	char filename[max_name_size];
	int err;
	strcpy(prog_sec_name, "-"); // default program section name

	while ((opt = getopt(argc, argv, optstr)) != -1) {
		switch (opt) {
		case 'S':
			xdp_flags |= XDP_FLAGS_SKB_MODE;
			break;
		case 'N':
			/* default, set below */
			break;
		case 'F':
			xdp_flags &= ~XDP_FLAGS_UPDATE_IF_NOEXIST;
			break;
		case 'p':
			strncpy(prog_sec_name, optarg, max_name_size);
			break;
		default:
			usage(basename(argv[0]));
			return 1;
		}
	}
	printf("XDP Program is '%s'\n", prog_sec_name);

	if (!(xdp_flags & XDP_FLAGS_SKB_MODE))
		xdp_flags |= XDP_FLAGS_DRV_MODE;

	printf("optind: %d, argc: %d\n", optind, argc);
	if (optind > argc - 2) {
		// Reached end of parameters but no interface
		usage(basename(argv[0]));
		return 1;
	}

	ifindex = if_nametoindex(argv[optind]);
	if (!ifindex) {
		perror("if_nametoindex");
		return 1;
	}

	strncpy(filename, argv[optind+1], sizeof(filename));
	printf("Binary object is '%s'\n", filename);
	//snprintf(filename, sizeof(filename), "%s_kern.o", argv[0]);
	prog_load_attr.file = filename;

	if (bpf_prog_load_xattr(&prog_load_attr, &obj, &prog_fd))
		return 1;

	if (prog_sec_name[0] != '-') {
		// If a program section name is provided then find that
		// otherwise use the first program in the object.
		prog = bpf_object__find_program_by_title(obj, prog_sec_name);
		prog_fd = bpf_program__fd(prog);
		if (!prog_fd) {
			printf("bpf_prog_load_xattr: %s\n", strerror(errno));
			return 1;
		}
	}

	signal(SIGINT, int_exit);
	signal(SIGTERM, int_exit);

	if (bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags) < 0) {
		printf("link set xdp fd failed\n");
		return 1;
	}

	err = bpf_obj_get_info_by_fd(prog_fd, &info, &info_len);
	if (err) {
		printf("can't get prog info - %s\n", strerror(errno));
		return err;
	}
	prog_id = info.id;


	wait();
	return 0;
}


static void wait(void)
{
	printf("Ctr-C to exit\n");
	while(1) sleep(1);
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"usage: %s [OPTS] IFACE BPF_OBJ_FILE\n\n"
		"OPTS:\n"
		"    -S    use skb-mode\n"
		"    -N    enforce native mode\n"
		"    -F    force loading prog\n"
		"    -p    program section name\n",
		prog);
}

static void int_exit(int sig)
{
	__u32 curr_prog_id = 0;

	if (bpf_get_link_xdp_id(ifindex, &curr_prog_id, xdp_flags)) {
		printf("bpf_get_link_xdp_id failed\n");
		exit(1);
	}
	if (prog_id == curr_prog_id)
		bpf_set_link_xdp_fd(ifindex, -1, xdp_flags);
	else if (!curr_prog_id)
		printf("couldn't find a prog id on a given interface\n");
	else
		printf("program on interface changed, not removing\n");
	exit(0);
}

