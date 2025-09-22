#include "pacc/pacc-fb.h"
#include "fmdsp/fmdsp-pacc.h"

#define EXPORT __attribute__((visibility("default")))

static struct {
  uint8_t fb[PC98_H * PC98_W * 4];
  struct pacc_ctx *pc;
  struct pacc_vtable pacc;
  struct fmdsp_pacc *fp;
} g = {0};

EXPORT bool fmplayer_web_init(void) {
  g.pc = pacc_init_fb(g.fb, PC98_W, PC98_H, &g.pacc);
  if (!g.pc) goto err;
  g.fp = fmdsp_pacc_alloc();
  if (!g.fp) goto err;
  if (!fmdsp_pacc_init(g.fp, g.pc, &g.pacc)) goto err;
  //fmdsp_pacc_palette(g.fp, 6);
  return true;
err:
  return false;
}

EXPORT uint8_t *fmplayer_web_get_fb(void) {
  return g.fb;
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
