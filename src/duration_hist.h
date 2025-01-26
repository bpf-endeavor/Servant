#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* hist[i] --> value in range(hist_grain*i, hist_grain*(i+1))
 * Maximum possible value = hist_max * hist_grain
 * */
#define _HIST_MAX 4096
static const float hist_grain = 10;
static const uint32_t hist_max = _HIST_MAX;
static uint64_t hist[_HIST_MAX] = {};
static uint64_t total_entries;

static int calc_latency_from_ts(uint64_t send_ts, uint64_t recv_ts)
{
  uint64_t now = recv_ts;
  uint64_t delta = now - send_ts;
  if (delta < 0) {
    printf("calculating latency negative value\n");
    return -1;
  }
  uint32_t index = delta / hist_grain;
  if (index > _HIST_MAX)
    index = _HIST_MAX;
  hist[index] += 1;
  total_entries++;
  return 0;
}

static void print_latency_result(void)
{
  float p[] = {0.01, 0.5, 0.99, 0.999};
  const int count_p = sizeof(p) / sizeof(float);
  uint64_t target = 0;
  uint64_t counter = 0;
  printf("\nLatency report (samples: %ld):\n", total_entries);
  for (int i = 0; i < count_p; i++) {
    counter = 0;
    target = (uint64_t)(total_entries * p[i]);
    int j;
    for (j = 0; j < hist_max; j++) {
      counter += hist[j];
      if (counter >= target) {
        break;
      }
    }
    printf("@%.1f: %.2f\n", p[i] * 100, (j * hist_grain + hist_grain / 2.0));
  }
}
