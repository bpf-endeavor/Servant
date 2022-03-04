/* This programs extracts the throughput measurments from the map */
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <linux/bpf.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <time.h>

static inline uint64_t get_ns(void) {
	struct timespec spec = {};
	clock_gettime(CLOCK_MONOTONIC, &spec);
	uint64_t rprt_ts = spec.tv_sec * 1000000000LL + spec.tv_nsec;
	return rprt_ts;
}

int main(int argc, char *argv[])
{
	// What map to look for
	char target[] = "tput";
	int target_id = -1;

	if (argc > 1)
		target_id = atoi(argv[1]);

	uint32_t id = 0;
	int ret = 0;
	struct bpf_map_info map_info = {};
	uint32_t info_size = sizeof(map_info);
	int map_fd = 0;
	int found = 0;
	const int zero = 0;

	long lastReport = 0;
	long curReport = 0;
	uint64_t lastTs = 0;
	uint64_t now = 0;
	uint64_t delta = 0;
	long value = 0;
	float tput = 0.0f;

	// Find the map
	while (!ret) {
		ret = bpf_map_get_next_id(id, &id);
		if (ret) {
			if (errno == ENOENT)
				break;
			printf("can't get next map: %s%s", strerror(errno),
					errno == EINVAL ? " -- kernel too old?" : "");
			break;
		}
		map_fd = bpf_map_get_fd_by_id(id);
		bpf_obj_get_info_by_fd(map_fd, &map_info, &info_size);
		printf("checking map: id: %d fd: %d name: %s\n", id, map_fd, map_info.name);
		if (!strcmp(target, map_info.name) || target_id == id) {
			printf("* map id: %d map name: %s\n", id, map_info.name);
			// found the map
			found = 1;
			break;
		}
	}

	if (found == 0) {
		printf("could not find the map\n");
		return 1;
	}

	printf("Reporting throughput\n");
	for (;;) {
		now = get_ns();
		delta =  (now - lastTs);
		if (delta < 2000000000LL) {
			sleep(1);
			/* printf("%ld\n", delta); */
			continue;
		}
		if (bpf_map_lookup_elem(map_fd, &zero, &value)) {
			printf("Failed to lookup the map\n");
		}
		curReport = value;
		tput = (curReport - lastReport) * 1000000000L/ (float)delta;
		lastTs = now;
		lastReport = curReport;
		printf("tput: %.2f\n", tput);
	}
	return 0;
}
