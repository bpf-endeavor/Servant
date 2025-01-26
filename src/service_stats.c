#include <time.h>

#include "service_stats.h"
#include "defs.h"
#include "config.h"
#include "log.h"


void *report(void *arg)
{
  int ret = 0;
  struct xsk_socket_info *xsk = arg;
  // struct timespec before, after;
  uint64_t before_pkts_rx;
  uint64_t before_pkts_tx;
  uint64_t duration;
  double rxpps;
  double txpps;
  while (!config.terminate) {
    ret = 1;
    duration = 1;
    struct timespec req = { .tv_sec = duration, .tv_nsec = 0};
    before_pkts_rx = xsk->ring_stats.rx_npkts;
    before_pkts_tx = xsk->ring_stats.tx_npkts;
    // clock_gettime(MONOTONIC_CLOCK, &before);
    while (ret != 0) {
      ret = nanosleep(&req, &req);
    }
    // clock_gettime(MONOTONIC_CLOCK, &after);
    rxpps = (xsk->ring_stats.rx_npkts - before_pkts_rx) / duration;
    txpps = (xsk->ring_stats.tx_npkts - before_pkts_tx) / duration;
    // printf("%srx: %.2f\ntotal: %ld\n", "\e[1;1H\e[2J", pps, xsk->ring_stats.rx_npkts);
    printf("%srx: %.2f\ttotal: %ld\n", "", rxpps, xsk->ring_stats.rx_npkts);
    printf("%stx: %.2f\ttotal: %ld\n", "", txpps, xsk->ring_stats.tx_npkts);
    printf("tmp: %d\n", config.tmp);
    printf("-----------------------------------------\n");
  }
  return 0;
}
