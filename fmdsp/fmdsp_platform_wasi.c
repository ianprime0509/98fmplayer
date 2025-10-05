#include "fmdsp_platform_info.h"
#include <time.h>
#include <limits.h>
#include <stdint.h>

int fmdsp_cpu_usage(void) {
  // There is no way to get CPU usage from JS.
  return 0;
}

int fmdsp_fps_30(void) {
  static struct timespec lasttimespec;

  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  uint64_t fps = 0;
  if (lasttimespec.tv_sec || lasttimespec.tv_nsec) {
    uint64_t diffns = time.tv_sec - lasttimespec.tv_sec;
    diffns *= 1000000000ull;
    diffns += time.tv_nsec - lasttimespec.tv_nsec;
    if (diffns) {
      fps = 30ull * 1000000000ull / diffns;
    }
  }
  lasttimespec = time;
  if (fps > INT_MAX) fps = INT_MAX;
  return fps;
}
