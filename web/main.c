#include "fmdsp/fmdsp-pacc.h"
#include "pacc-webgl.h"

#define EXPORT __attribute__((visibility("default")))

static struct {
  struct pacc_ctx *pc;
  struct pacc_vtable pacc;
  struct fmdsp_pacc *fp;
} g = {0};

EXPORT bool fmplayer_web_init(void) {
  g.pc = pacc_init_webgl(PC98_W, PC98_H, &g.pacc);
  if (!g.pc) goto err;
  g.fp = fmdsp_pacc_alloc();
  if (!g.fp) goto err;
  if (!fmdsp_pacc_init(g.fp, g.pc, &g.pacc)) goto err;
  return true;
err:
  return false;
}

EXPORT void fmplayer_web_render(void) {
  fmdsp_pacc_render(g.fp);
}

// TODO
int fmdsp_cpu_usage(void) {
  return 0;
}

// TODO
int fmdsp_fps_30(void) {
  return 0;
}
