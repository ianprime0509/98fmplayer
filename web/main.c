#include <stdatomic.h>

#include "common/fmplayer_common.h"
#include "common/fmplayer_file.h"
#include "libopna/opna.h"
#include "libopna/opnadrum.h"
#include "libopna/opnatimer.h"
#include "fmdriver/fmdriver.h"
#include "fft/fft.h"
#include "fmdsp/fmdsp-pacc.h"
#include "fmdsp/font.h"
#include "fmdsp/fontrom_shinonome.inc"
#include "pacc-webgl.h"

#define EXPORT(name) __attribute__((export_name(name)))

enum {
  MAX_SAMPLES = 128,
};

static uint8_t opna_drum_rom[OPNA_ROM_SIZE] = {
#if __has_embed("ym2608_adpcm_rom.bin")
#embed "ym2608_adpcm_rom.bin"
#endif
};

static struct {
  atomic_flag opna_flag;
  struct opna opna;
  struct opna_timer opna_timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  char adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  struct fmplayer_file fmfile;
  uint8_t fmfile_data[0xffff];
  struct fmdsp_font font98;
  atomic_flag at_fftdata_flag;
  struct fmplayer_fft_data at_fftdata;
  struct fmplayer_fft_input_data fftdata;
  struct pacc_ctx *pc;
  struct pacc_vtable pacc;
  struct fmdsp_pacc *fp;
  int16_t audio_buf[MAX_SAMPLES * 2];
} g = {
  .opna_flag = ATOMIC_FLAG_INIT,
  .at_fftdata_flag = ATOMIC_FLAG_INIT,
};

EXPORT("init") bool fmplayer_web_init(void) {
  fft_init_table();
  fmplayer_init_work_opna(&g.work, &g.ppz8, &g.opna, &g.opna_timer, g.adpcm_ram);

  g.pc = pacc_init_webgl(PC98_W, PC98_H, &g.pacc);
  if (!g.pc) goto err;
  g.fp = fmdsp_pacc_alloc();
  if (!g.fp) goto err;
  if (!fmdsp_pacc_init(g.fp, g.pc, &g.pacc)) goto err;
  fmdsp_font_from_font_rom(&g.font98, fmdsp_shinonome_font_rom);
  fmdsp_pacc_set_font16(g.fp, &g.font98);
  fmdsp_pacc_set(g.fp, &g.work, &g.opna, &g.fftdata);
  return true;
err:
  return false;
}

EXPORT("getFileBuf") uint8_t *fmplayer_web_get_file_buf(void) {
  return g.fmfile_data;
}

EXPORT("loadFile") bool fmplayer_web_load_file(size_t len) {
  // TODO: this is very bare bones
  while (atomic_flag_test_and_set_explicit(&g.opna_flag, memory_order_acquire));
  if (!pmd_load(&g.fmfile.driver.pmd, g.fmfile_data, len)) goto err;
  pmd_init(&g.work, &g.fmfile.driver.pmd);
  g.work.pcmerror[0] = true;
  g.work.pcmerror[1] = true;
  g.work.pcmerror[2] = true;
  atomic_flag_clear_explicit(&g.opna_flag, memory_order_release);

  fmdsp_pacc_update_file(g.fp);
  fmdsp_pacc_comment_reset(g.fp);

  return true;
err:
  atomic_flag_clear_explicit(&g.opna_flag, memory_order_release);
  return false;
}

EXPORT("render") void fmplayer_web_render(void) {
  if (!atomic_flag_test_and_set_explicit(&g.at_fftdata_flag, memory_order_acquire)) {
    memcpy(&g.fftdata.fdata, &g.at_fftdata, sizeof(g.fftdata.fdata));
    atomic_flag_clear_explicit(&g.at_fftdata_flag, memory_order_release);
  }
  fmdsp_pacc_render(g.fp);
}

EXPORT("getAudioBuf") int16_t *fmplayer_web_get_audio_buf(void) {
  return g.audio_buf;
}

EXPORT("mix") void fmplayer_web_mix(size_t samples) {
  while (atomic_flag_test_and_set_explicit(&g.opna_flag, memory_order_acquire));
  memset(g.audio_buf, 0, sizeof(g.audio_buf));
  opna_timer_mix(&g.opna_timer, g.audio_buf, samples);
  atomic_flag_clear_explicit(&g.opna_flag, memory_order_release);

  if (!atomic_flag_test_and_set_explicit(&g.at_fftdata_flag, memory_order_acquire)) {
    fft_write(&g.at_fftdata, g.audio_buf, samples);
    atomic_flag_clear_explicit(&g.at_fftdata_flag, memory_order_release);
  }
}

// TODO
int fmdsp_cpu_usage(void) {
  return 0;
}

// TODO
int fmdsp_fps_30(void) {
  return 0;
}

bool fmplayer_drum_rom_load(struct opna_drum *drum) {
  opna_drum_set_rom(drum, opna_drum_rom);
  return true;
}
