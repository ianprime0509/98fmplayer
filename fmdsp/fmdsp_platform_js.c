#include "fmdsp_platform_info.h"
#include <limits.h>
#include <stdint.h>

#define IMPORT(name) __attribute__((import_module("fmdsp_platform"), import_name(name)))

IMPORT("cpuUsage") extern int fmdsp_platform_js_cpu_usage(void);
IMPORT("nanotime") extern uint64_t fmdsp_platform_js_nanotime(void);

int fmdsp_cpu_usage(void) {
  return fmdsp_platform_js_cpu_usage();
}

int fmdsp_fps_30(void) {
  static uint64_t lasttime;

  uint64_t time = fmdsp_platform_js_nanotime();
  uint64_t fps = 30ull * 1000000000ull / (time - lasttime);
  lasttime = time;
  if (fps > INT_MAX) fps = INT_MAX;
  return fps;
}
