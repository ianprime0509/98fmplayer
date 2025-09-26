#include "common/fmplayer_file.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "fmdriver/fmdriver.h"
#include "fft/fft.h"
#include "fmdsp/fmdsp-pacc.h"
#include "pacc-webgl.h"

#define EXPORT(name) __attribute__((export_name(name)))

static struct {
  struct opna opna;
  struct opna_timer opna_timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  char adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  struct fmplayer_file fmfile;
  uint8_t fmfile_data[0xffff];
  struct fmplayer_fft_input_data fftdata;
  struct pacc_ctx *pc;
  struct pacc_vtable pacc;
  struct fmdsp_pacc *fp;
} g = {0};

EXPORT("init") bool fmplayer_web_init(void) {
  g.pc = pacc_init_webgl(PC98_W, PC98_H, &g.pacc);
  if (!g.pc) goto err;
  g.fp = fmdsp_pacc_alloc();
  if (!g.fp) goto err;
  if (!fmdsp_pacc_init(g.fp, g.pc, &g.pacc)) goto err;
  fmdsp_pacc_set(g.fp, &g.work, &g.opna, &g.fftdata);
  return true;
err:
  return false;
}

EXPORT("render") void fmplayer_web_render(void) {
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
