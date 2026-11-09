#include "fmplayer_backend.h"
#include "fmplayer_common.h"
#include "fmdriver/fmdriver.h"
#include "fmdriver/ppz8.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"

struct libopna_state {
  struct opna opna;
  struct opna_timer timer;
};

static void libopna_init(void *storage, struct ppz8 *ppz8,
                         struct fmdriver_work *work, void *adpcm_ram) {
  struct libopna_state *st = storage;
  fmplayer_init_work_opna(work, ppz8, &st->opna, &st->timer, adpcm_ram);
  opna_ssg_set_mix(&st->opna.ssg, 0x10000);
  opna_ssg_set_ymf288(&st->opna.ssg, &st->opna.resampler, false);
  ppz8_set_interpolation(ppz8, PPZ8_INTERP_SINC);
  opna_fm_set_hires_sin(&st->opna.fm, false);
  opna_fm_set_hires_env(&st->opna.fm, false);
}

static void libopna_mix(void *storage, int16_t *buf, unsigned samples) {
  struct libopna_state *st = storage;
  opna_timer_mix(&st->timer, buf, samples);
}

const struct fmplayer_backend fmplayer_backend_libopna = {
  .name = "libopna",
  .sample_rate = 55467,
  .ppz8_mix = 0xa000,
  .storage_size = sizeof(struct libopna_state),
  .init = libopna_init,
  .mix = libopna_mix,
};
