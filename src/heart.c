/**
 * This function has the function that interface with AF_XDP. Receiving packets
 * and pumping them to the eBPF engine (Brain)
 */
#include <assert.h>
#include <arpa/inet.h>
#include <stdlib.h> // exit
#include <sys/socket.h> // sendto
#include <errno.h>
#include <time.h>
#include "config.h"
#include "heart.h"
#include "brain.h"
#include "log.h"
#include "include/packet_context.h"


/* #define USE_POLL */
#define SHOW_THROUGHPUT
/* #define VM_CALL_BATCHING */
/* #define REPORT_UBPF_OVERHEAD */
/* #define DBG_CHECK_INCOMING_PKTS */


#ifdef USE_POLL
#pragma message "POLL is enabled"
#include <poll.h>
#endif


#ifdef REPORT_UBPF_OVERHEAD
#include "duration_hist.h"
#include <x86intrin.h>
static inline
uint64_t readTSC(void) {
  // _mm_lfence();  // optionally wait for earlier insns to retire before reading the clock
  uint64_t tsc = __rdtsc();
  // _mm_lfence();  // optionally block later instructions until rdtsc retires
  return tsc;
}
#endif

#define ABS(x) (((x) < 0) ? -(x) : (x))

#include "xsk_actions.h"

#define F_HAS_YIELDED (1 << 7)
#define PROG_INDEX_MASK 0x7
#define YIELD_MASK 0xffff
#define YIELD_PROG_INDEX(y) ((y >> 16) & PROG_INDEX_MASK)
#define MAX_NUM_STAGE 8
static uint8_t has_yield = 0;
static uint8_t yield_state[128];
static uint8_t stage_number = 0;

#ifdef DBG_CHECK_INCOMING_PKTS
#include <linux/if_ether.h>
#include <linux/ip.h>
static int check_is_for_this_server(void *ctx)
{
  static const uint8_t mac[6] = {0x9c,0xdc,0x71,0x5e,0x0f,0x41};
  static const uint32_t server_ip = 0x0101a8c0; // 0xC0A80101
  static char tmp_mac[32];
  struct ethhdr *eth = ctx;
  struct iphdr *ip = (struct iphdr *)(eth + 1);
  /* if (eth->h_proto != htons(ETH_P_IP) || ip->protocol != IPPROTO_UDP) { */
  if (eth->h_proto != htons(ETH_P_IP) || ip->protocol != IPPROTO_TCP) {
    DEBUG("unexpected inner protocol! (eth: %d ip: %d)\n", eth->h_proto, ip->protocol);
    return -1;
  }
  if (memcmp(eth->h_dest, mac, 6) != 0) {
    snprintf(tmp_mac, 31, "%x:%x:%x:%x:%x:%x",
        eth->h_dest[0],
        eth->h_dest[1],
        eth->h_dest[2],
        eth->h_dest[3],
        eth->h_dest[4],
        eth->h_dest[5]
        );
    DEBUG("wrong mac address: %s\n", tmp_mac);
    DEBUG("\tip: %d.%d.%d.%d\n",
        (ip->daddr) & 0xff,
        (ip->daddr >> 8) & 0xff,
        (ip->daddr >> 16) & 0xff,
        (ip->daddr >> 24) & 0xff
        );
    return -1;
  }
  if (ip->daddr != server_ip) {
    DEBUG("wrong ip %x != %x\n", ip->daddr, server_ip);
    return -1;
  }
  return 0;
}
#endif

/**
 * Poll rx queue of the socket and send received packets to eBPF engine
 *
 * This function has an infinte loop that stops when terminate flag is raised.
 *
 * @param xsk Socket to be polled.
 */
  void
pump_packets(struct xsk_socket_info *xsk, struct ubpf_vm *vm)
{
  int ret;
#ifdef SHOW_THROUGHPUT
  static uint64_t pkt_count = 0;
  static uint64_t sent_count = 0;
  struct timespec spec = {};
  clock_gettime(CLOCK_MONOTONIC_COARSE, &spec);
  uint64_t rprt_ts = spec.tv_sec * 1000000 + spec.tv_nsec / 1000;
#endif

  const uint32_t cnt = config.batch_size;
  if (ubpf_set_batch_size(cnt) != 0) {
    ERROR("in uBPF the MAX_BATCH_SZ is set to 128 (for holding the state when yielding)\n");
    return;
  }
  struct xdp_desc batch[128];
  struct pktctx _pkt_ctx_arr[128];
  verdict_t _ret_arr[128];
  memset(batch, 0, sizeof(batch));
  memset(_pkt_ctx_arr, 0, sizeof(_pkt_ctx_arr));
  memset(_ret_arr, 0, sizeof(_ret_arr));

  struct pktctxbatch pkt_batch;
  pkt_batch.cnt  = 0;
  pkt_batch.pkts = _pkt_ctx_arr;
  pkt_batch.rets = _ret_arr;

#ifdef USE_POLL
  struct pollfd fds[1] = {};
  uint32_t empty_rx = 0;
#endif

  if (!config.jitted) {
    ERROR("Configureation disabled JIT.\nIntentionally use JITted mode (ignore the flag)\n");
  }
  char *errmsg;
  ubpf_jit_fn bpf_progs[MAX_NUM_PROGS];
  for (int i = 0; i < config.yield_sz; i++) {
    bpf_progs[i] = ubpf_compile(vm, i, &errmsg);
    if (bpf_progs[i] == NULL) {
      ERROR("Failed to compile: %s\n", errmsg);
      free(errmsg);
      return;
    }
  }

  /* TODO: this is just a hack ... */
  void *_tmp = ubpf_select_map("server_id_map", vm);
  uint32_t _zero = 0;
  uint32_t *server_id_map_base = ubpf_lookup_map(_tmp, &_zero);
  /* --------------------------------------------------------------- */

  void *meta[cnt];
  for (int i = 0; i < cnt; i++) {
    meta[i] = malloc(SERVANT_METADATA_SIZE);
    if (meta[i] == NULL) {
      ERROR("Out of memory while allocating metadata region\n");
      for (int j = 0; j < i; j++)
        free(meta[j]);
      return;
    }
    pkt_batch.pkts[i].meta = meta[i];
    /* TODO: the next line is just a hack to pass an address to
     * Katran with 3phase lookup optimization */
    pkt_batch.pkts[i].help.server_id_map_base =
      (uint64_t)server_id_map_base;
    /* --------------------------------------------------------- */
  }

  for(;;) {
    if (config.terminate)
      break;

    if (xsk->outstanding_tx > 0)
      complete_tx(xsk);

#ifdef USE_POLL
    if (empty_rx >= 8) {
      fds[0].fd = xsk_socket__fd(xsk->xsk);
      fds[0].events = POLLIN; // POLLOUT |
      if (poll(fds, 1, 1000) <= 0)
        continue;
      else
        empty_rx = 0;
    }
#endif

    const uint32_t rx = poll_rx_queue(xsk, batch, cnt);
    if (!rx) {
#ifdef USE_POLL
      empty_rx++;
#endif
      continue;
    }

    /* Received a new batch of packets. reset the state */
    stage_number = 0;

    /* Perpare batch */
    pkt_batch.cnt = rx;

    // Initialize a batch of structures that we pass to eBPF env
    for (uint32_t i = 0; i < rx; i++) {
      const uint64_t addr = batch[i].addr;
      const size_t ctx_len = batch[i].len;
      /* DEBUG("addr: %ld (%ld)- %ld\n", addr, xsk_umem__add_offset_to_addr(addr), ctx_len); */
      void *const ctx = xsk_umem__get_data(xsk->umem->buffer, addr);
      pkt_batch.pkts[i].data = ctx;
      pkt_batch.pkts[i].data_end = ctx + ctx_len;
      pkt_batch.pkts[i].pkt_len = ctx_len;
      pkt_batch.pkts[i].trim_head = 0;
      pkt_batch.rets[i] = 0;
      /* Consider this packet for processing and start from program at zero index */
      yield_state[i] = F_HAS_YIELDED;
#ifdef DBG_CHECK_INCOMING_PKTS
      if (check_is_for_this_server(ctx) != 0) {
        pkt_batch.rets[i] = DROP;
        /* terminated before the first stage */
        yield_state[i] = 0; // Ignore processing of this program
      }
#endif
      /* Make sure the program state machine is clean */
      /* memset(pkt_batch.pkts[i].meta, 0, SERVANT_INTER_STAGE_STATE_SIZE); */
    }

    /* Start of the yield-chain */
    do {
      has_yield = 0;

      for (uint32_t i = 0; i < rx; i++) {
        ubpf_set_batch_offset(i);
        if ((yield_state[i] & F_HAS_YIELDED) == 0) {
          /* The verdict is known */
          continue;
        }
        struct pktctx *const pktctx = &pkt_batch.pkts[i];

#ifdef REPORT_UBPF_OVERHEAD
        uint64_t start_ts = readTSC();
#endif

        const uint8_t prog_index = yield_state[i] & PROG_INDEX_MASK;
        const ubpf_jit_fn fn = bpf_progs[prog_index];
        pkt_batch.rets[i] = fn(pktctx, sizeof(*pktctx));

#ifdef REPORT_UBPF_OVERHEAD
        uint64_t end_ts = readTSC();
        calc_latency_from_ts(start_ts, end_ts);
#endif
        if (pktctx->pkt_len > config.frame_size) {
          ERROR("Too large packet len!\n");
          pkt_batch.rets[i] = DROP;
        } else {
          batch[i].len = pktctx->pkt_len;
        }
        if (ABS(pktctx->trim_head) > config.headroom) {
          ERROR("trim_head is larger than the configured headroom size\n");
          pkt_batch.rets[i] = DROP;
        } else  {
          if (pktctx->trim_head != 0) {
            DEBUG("trim head = %d\n", pktctx->trim_head);
          }
          batch[i].addr += pktctx->trim_head;
        }

        switch (pkt_batch.rets[i]) {
          case DROP:
            xsk->batch.drop++;
            break;
          case SEND:
            xsk->batch.tx++;
            break;
          case PASS:
            /* will eventually put the descriptor on the fill queue */
            xsk->batch.drop++;
            break;
          default:
            if ((pkt_batch.rets[i] & YIELD_MASK) == YIELD) {
              /* Update the states if the VM has yielded */
              /* move to the new stage */
              const uint8_t next_prog = YIELD_PROG_INDEX(pkt_batch.rets[i]);
              yield_state[i] = next_prog | F_HAS_YIELDED;
              has_yield = 1;
            } else {
              /* Unknown verdict */
              xsk->batch.drop++;
            }
            break;
        }
      }
      stage_number++;
    } while(has_yield && stage_number < MAX_NUM_STAGE);

    if (has_yield) {
      ERROR("Found yield in the last stage!\n");
      has_yield = 0;
      for (uint32_t i = 0; i < rx; i++) {
        if ((pkt_batch.rets[i] & YIELD_MASK) == YIELD) {
          pkt_batch.rets[i] = DROP;
          xsk->batch.drop++;
        }
      }
    }

    /* reserve the descriptors on the rings */
    uint32_t fq_index = 0;
    uint32_t tx_index = 0;
    do {
_repeat_fq:
      ret = xsk_ring_prod__reserve(&xsk->umem->fq,
          xsk->batch.drop, &fq_index);
      if (ret != xsk->batch.drop) {
        kick_rx(xsk);
        goto _repeat_fq;
      }
    } while(0);
    do {
_repeat_tx:
      ret = xsk_ring_prod__reserve(&xsk->tx,
          xsk->batch.tx, &tx_index);
      if (ret != xsk->batch.tx) {
        kick_tx(xsk);
        goto _repeat_tx;
      }
    } while(0);

    /* Actually place the descriptors on the ring */
    for (uint32_t i = 0; i < rx; i++) {
      switch (pkt_batch.rets[i]) {
        case SEND:
          *xsk_ring_prod__tx_desc(&xsk->tx, tx_index++) = batch[i];
          break;
        case PASS:
          do_pass(xsk, &batch[i]);
          *xsk_ring_prod__fill_addr(&xsk->umem->fq, fq_index++) = CHUNK_ALIGN(batch[i].addr);
          break;
        default:
          /* ERROR("Unexpected verdict\n"); */
          /* fallthrough */
        case DROP:
          *xsk_ring_prod__fill_addr(&xsk->umem->fq, fq_index++) = CHUNK_ALIGN(batch[i].addr);
          break;
      }
    }
    xsk_ring_prod__submit(&xsk->tx, xsk->batch.tx);
    xsk_ring_prod__submit(&xsk->umem->fq, xsk->batch.drop);
    xsk->outstanding_tx += xsk->batch.tx;
#ifdef SHOW_THROUGHPUT
    sent_count += xsk->batch.tx;
#endif
    xsk->batch.drop = 0;
    xsk->batch.tx = 0;
    release_rx_queue(xsk, rx);

#ifdef SHOW_THROUGHPUT
    pkt_count += rx;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &spec);
    uint64_t now = spec.tv_sec * 1000000 + spec.tv_nsec / 1000;
    uint64_t delta = now - rprt_ts;
    if (delta > 2000000) {
      INFO("TP: %d send: %d\n", pkt_count * 1000000 /
          delta, sent_count * 1000000 /
          delta);
      pkt_count = 0;
      sent_count = 0;
      rprt_ts = now;
    }
#endif
  }

  for (int i = 0; i < cnt; i++) {
    free(meta[i]);
  }
#ifdef REPORT_UBPF_OVERHEAD
  print_latency_result();
#endif
}
