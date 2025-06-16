#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sndfile.h>
#include "common/fmplayer_common.h"
#include "common/fmplayer_file.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"

enum {
  SRATE = 55467,
};

struct fmplayer_config {
  bool fm_hires_env;
  bool fm_hires_sin;
  bool ssg_ymf288;
  uint32_t ssg_mix;
  enum ppz8_interp ppz8_interp;
};

struct fadeout {
  struct opna_timer *timer;
  struct fmdriver_work *work;
  uint64_t vol;
  uint8_t loopcnt;
  bool enabled;
};

static bool fadeout_mix(
  struct fadeout *fadeout,
  int16_t *buf, unsigned frames
) {
  opna_timer_mix(fadeout->timer, buf, frames);
  if (fadeout->enabled) {
    for (unsigned i = 0; i < frames; i++) {
      int vol = fadeout->vol >> 16;
      buf[i*2+0] = (buf[i*2+0] * vol) >> 16;
      buf[i*2+1] = (buf[i*2+1] * vol) >> 16;
      if (fadeout->work->loop_cnt >= fadeout->loopcnt) {
        fadeout->vol = (fadeout->vol * 0xffff0000ull) >> 32;
      }
    }
    return fadeout->vol;
  } else {
    return fadeout->work->loop_cnt < fadeout->loopcnt;
  }
}

static void help(const char *name) {
  fprintf(stderr, "Usage: %s [options] input output\n", name);
  fprintf(stderr, "  options:\n");
  fprintf(stderr, "  -h        show help\n");
  fprintf(stderr, "  -l loops  set loop count\n");
  fprintf(stderr, "  -n        disable fadeout\n");
  exit(1);
}

int main(int argc, char **argv) {
  struct fmplayer_config fmplayer_config = {
    .ssg_mix = 0x10000,
    .ppz8_interp = PPZ8_INTERP_SINC,
  };
  struct fadeout fadeout = {
    .vol = 1ull<<32,
    .loopcnt = 1,
    .enabled = true,
  };

  int optchar;
  while ((optchar = getopt(argc, argv, "hl:n")) != -1) {
    switch (optchar) {
    case 'l':
      fadeout.loopcnt = atoi(optarg);
      break;
    case 'n':
      fadeout.enabled = false;
      break;
    default:
    case 'h':
      help(argv[0]);
      break;
    }
  }
  if (argc != optind + 2) {
    fprintf(stderr, "invalid arguments\n");
    help(argv[0]);
  }
  const char *input_filename = argv[optind];
  const char *output_filename = argv[optind + 1];

  enum fmplayer_file_error error;
  struct fmplayer_file *fmfile = fmplayer_file_alloc(input_filename, &error);
  if (!fmfile) {
    fprintf(stderr, "cannot load file: %s\n", fmplayer_file_strerror(error));
    return 1;
  }

  SF_INFO sfinfo = {
    .samplerate = SRATE,
    .channels = 2,
    .format = SF_FORMAT_WAV | SF_ENDIAN_CPU | SF_FORMAT_PCM_16,
    .seekable = 1,
  };
  SNDFILE *sndfile = sf_open(output_filename, SFM_WRITE, &sfinfo);
  if (!sndfile) {
    fprintf(stderr, "cannot open output file: %s\n", sf_strerror(sndfile));
    fmplayer_file_free(fmfile);
    return 1;
  }

  struct opna opna;
  struct opna_timer timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  uint8_t adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  fmplayer_init_work_opna(&work, &ppz8, &opna, &timer, &adpcm_ram);
  opna_ssg_set_mix(&opna.ssg, fmplayer_config.ssg_mix);
  opna_ssg_set_ymf288(&opna.ssg, &opna.resampler, fmplayer_config.ssg_ymf288);
  ppz8_set_interpolation(&ppz8, fmplayer_config.ppz8_interp);
  opna_fm_set_hires_sin(&opna.fm, fmplayer_config.fm_hires_sin);
  opna_fm_set_hires_env(&opna.fm, fmplayer_config.fm_hires_env);
  fmplayer_file_load(&work, fmfile, fadeout.loopcnt);
  fadeout.timer = &timer;
  fadeout.work = &work;

  enum {
    BUFLEN = 1024,
  };
  int16_t buf[BUFLEN*2];
  for (;;) {
    memset(buf, 0, sizeof(buf));
    bool end = !fadeout_mix(&fadeout, buf, BUFLEN);
    if (sf_writef_short(sndfile, buf, BUFLEN) != BUFLEN) {
      fprintf(stderr, "cannot write to output file: %s\n", sf_strerror(sndfile));
      sf_close(sndfile);
      fmplayer_file_free(fmfile);
      return 1;
    }
    if (end) break;
  }

  sf_close(sndfile);
  fmplayer_file_free(fmfile);
  return 0;
}
