#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <locale.h>
#include <setjmp.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <iconv.h>
#include <langinfo.h>

#include <SDL3/SDL.h>
#include <mc.h>

#include "common/fmplayer_common.h"
#include "common/fmplayer_file.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"

enum {
  SRATE = 55467,
  CHANNELS = 2,
  BLOCK_FRAMES = 1024,
};

enum {
  VOLUME_INIT = 0x100000000,
  VOLUME_FADE = 0x4000,
};

static const char *usage =
  "Usage: %s [OPTION...] FILE\n"
  "Play PMD or FMP modules, or compile and play a PMD MML file.\n"
  "\n"
  "Options:\n"
  "  -h, --help           show help\n"
  "  -F, --no-fade        do not fade out at end\n"
  "  -l, --loops=LOOPS    play song LOOPS times (default: 1)\n"
  "  -o, --output=OUTPUT  write output in WAV format to OUTPUT\n";

static const struct option options[] = {
  { .name = "help",       .has_arg = no_argument,       .val = 'h' },
  { .name = "no-fade",    .has_arg = no_argument,       .val = 'F' },
  { .name = "loops",      .has_arg = required_argument, .val = 'l' },
  { .name = "output",     .has_arg = required_argument, .val = 'o' },
  {},
};

struct mix_context {
  struct opna_timer *timer;
  struct fmdriver_work *work;
  uint64_t volume;
  uint8_t loops;
  bool fadeout_enabled;
  atomic_bool playing;
};

static bool mix_audio(int16_t *out, size_t frames, struct mix_context *ctx) {
  memset(out, 0, CHANNELS * sizeof(int16_t) * frames);
  opna_timer_mix(ctx->timer, out, frames);
  if (ctx->fadeout_enabled && ctx->work->loop_cnt >= ctx->loops) {
    for (unsigned long i = 0; i < frames; i++) {
      int volume = ctx->volume >> 16;
      out[2 * i + 0] = (out[2 * i + 0] * volume) >> 16;
      out[2 * i + 1] = (out[2 * i + 1] * volume) >> 16;
      ctx->volume = ctx->volume > VOLUME_FADE ? ctx->volume - VOLUME_FADE : 0;
    }
    return ctx->volume > 0;
  } else {
    return ctx->work->loop_cnt < ctx->loops;
  }
}

static void audiocb(
  void *userdata,
  SDL_AudioStream *stream,
  int additional_amount,
  int total_amount) {
  struct mix_context *ctx = userdata;
  int needed_frames = additional_amount / CHANNELS / sizeof(int16_t);
  while (needed_frames > 0) {
    int16_t buf[BLOCK_FRAMES * CHANNELS];
    int frames = needed_frames >= BLOCK_FRAMES ? BLOCK_FRAMES : needed_frames;
    int len = frames * CHANNELS * sizeof(int16_t);
    memset(buf, 0, len);
    bool has_more = mix_audio(buf, frames, ctx);
    SDL_PutAudioStreamData(stream, buf, len);
    needed_frames -= frames;
    if (!has_more) {
      atomic_store_explicit(&ctx->playing, false, memory_order_release);
      return;
    }
  }
}

struct mc_sys_context {
  int cwd;
  jmp_buf exit_jmp_buf;
};

static void mc_sys_putc(char c, void *user_data) {
  (void)user_data;
  putc(c, stderr);
}

static void mc_sys_print(const char *mes, void *user_data) {
  (void)user_data;
  fprintf(stderr, "%s", mes);
}

static void *mc_sys_create(const char *filename, void *user_data) {
  struct mc_sys_context *ctx = user_data;
  int fd = openat(ctx->cwd, filename, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd == -1) return 0;
  return fdopen(fd, "wb");
}

static void *mc_sys_open(const char *filename, void *user_data) {
  struct mc_sys_context *ctx = user_data;
  int fd = openat(ctx->cwd, filename, O_RDONLY);
  if (fd == -1) return 0;
  return fdopen(fd, "rb");
}

static int mc_sys_close(void *file, void *user_data) {
  (void)user_data;
  return fclose(file);
}

static int mc_sys_read(void *file, void *dest, uint16_t n, uint16_t *read, void *user_data) {
  (void)user_data;
  *read = fread(dest, 1, n, file);
  return ferror(file);
}

static int mc_sys_write(void *file, void *data, uint16_t n, void *user_data) {
  (void)user_data;
  return fwrite(data, 1, n, file) < n;
}

PMDC_NORETURN static void mc_sys_exit(int status, void *user_data) {
  struct mc_sys_context *ctx = user_data;
  longjmp(ctx->exit_jmp_buf, status + 1);
}

static char *mc_sys_getenv(const char *name, void *user_data) {
  (void)user_data;
  return getenv(name);
}

static const struct mc_sys mc_sys = {
  .putc = mc_sys_putc,
  .print = mc_sys_print,
  .create = mc_sys_create,
  .open = mc_sys_open,
  .close = mc_sys_close,
  .read = mc_sys_read,
  .write = mc_sys_write,
  .exit = mc_sys_exit,
  .getenv = mc_sys_getenv,
};

static bool is_mml(const char *filename) {
  size_t len = strlen(filename);
  return len >= 4 &&
    filename[len - 4] == '.' &&
    toupper(filename[len - 3]) == 'M' &&
    toupper(filename[len - 2]) == 'M' &&
    toupper(filename[len - 1]) == 'L';
}

static void sjis_to_native(char *out, size_t out_left, const char *sjis) {
  assert(out_left > 0);
  iconv_t cd = iconv_open(nl_langinfo(CODESET), "CP932");
  if (cd == (iconv_t)-1) {
    *out = 0;
    return;
  }
  char *in = (char*)sjis;
  out_left--; // Ensure we have enough room for a null terminator
  size_t in_left = strlen(in);
  for (;;) {
    if (iconv(cd, &in, &in_left, &out, &out_left) == (size_t)-1) {
      *out = 0;
      return;
    }
    if (in_left == 0) break;
  }
  iconv(cd, 0, 0, &out, &out_left);
  iconv_close(cd);
  *out = 0;
}

static void print_comments(struct fmdriver_work *work) {
  static const char *pmd_comment_titles[] = {
    "Title",
    "Composer",
    "Arranger",
  };
  for (int i = 0; ; i++) {
    const char *comment = work->get_comment(work, i);
    if (!comment) break;
    if (work->comment_mode_pmd && i < 3) {
      printf("%s: ", pmd_comment_titles[i]);
    }
    char memo[256];
    sjis_to_native(memo, sizeof(memo), comment);
    printf("%s\n", memo);
  }
}

static int compile(char **filename) {
  char *dirname = 0;
  DIR *dir = 0;
  int dir_fd = AT_FDCWD;
  char *fsep = strrchr(*filename, '/');
  if (fsep) {
    *fsep = 0;
    dirname = *filename;
    *filename = fsep + 1;
    if (!(dir = opendir(dirname))) {
      perror("cannot open MML directory");
      return 1;
    }
    if ((dir_fd = dirfd(dir)) == -1) {
      perror("cannot open MML directory");
      closedir(dir);
      return 1;
    }
  }

  struct mc_sys_context mc_sys_ctx = {
    .cwd = dir_fd,  
  };
  struct mc mc;
  mc.sys = &mc_sys;
  mc.user_data = &mc_sys_ctx;
  mc_init(&mc);
  switch (setjmp(mc_sys_ctx.exit_jmp_buf)) {
  case 0:
    mc_main(&mc, *filename);
    break;
  case 1:
    if (dirname) {
      size_t dirname_len = strlen(dirname);
      size_t m_file_len = strlen(mc.m_filename);
      *filename = malloc(dirname_len + 1 + m_file_len + 1);
      if (!*filename) {
        perror("");
        closedir(dir);
        return 1;
      }
      memcpy(*filename, dirname, dirname_len);
      (*filename)[dirname_len] = '/';
      memcpy(*filename + dirname_len + 1, mc.m_filename, m_file_len);
      (*filename)[dirname_len + 1 + m_file_len] = 0;
    } else {
      *filename = strdup(mc.m_filename);
      if (!*filename) {
        perror("");
        return 1;
      }
    }
    break;
  default:
    fprintf(stderr, "MML compilation failed\n");
    if (dir) closedir(dir);
    return 1;
  }

  if (dir) closedir(dir);
  return 0;
}

static int play(struct mix_context *ctx) {
  if (!SDL_Init(SDL_INIT_AUDIO)) {
    fprintf(stderr, "cannot initialize audio: %s\n", SDL_GetError());
    return 1;
  }
  SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
    SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
    &(SDL_AudioSpec){ .format = SDL_AUDIO_S16, .channels = CHANNELS, .freq = SRATE },
    audiocb,
    ctx);
  if (!stream) {
    fprintf(stderr, "cannot open audio stream: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  uint64_t start_time_ms = SDL_GetTicks();
  atomic_store_explicit(&ctx->playing, true, memory_order_release);
  if (!SDL_ResumeAudioStreamDevice(stream)) {
    fprintf(stderr, "cannot start audio stream: %s\n", SDL_GetError());
    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    return 1;
  }

  while (atomic_load_explicit(&ctx->playing, memory_order_acquire)) {
    uint64_t current_time_ms = SDL_GetTicks();
    uint64_t elapsed_time_s = (current_time_ms - start_time_ms) / 1000;
    printf("\rPlaying: %" PRIu64 ":%02" PRIu64, elapsed_time_s / 60, elapsed_time_s % 60);
    fflush(stdout);
    SDL_WaitEventTimeout(0, 1000);
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) goto quit;
    }
  }
quit:
  printf("\n");

  SDL_DestroyAudioStream(stream);
  SDL_Quit();
  return 0;
}

static void w16le(uint8_t *out, uint16_t x) {
  out[0] = x;
  out[1] = x >> 8;
}

static void w32le(uint8_t *out, uint32_t x) {
  out[0] = x;
  out[1] = x >> 8;
  out[2] = x >> 16;
  out[3] = x >> 24;
}

static int save(const char *output, struct mix_context *ctx) {
  size_t wav_bytes = 0;
  size_t wav_bytes_cap = 1024 * sizeof(int16_t) * CHANNELS * BLOCK_FRAMES;
  uint8_t *wav_data = malloc(wav_bytes_cap);
  if (!wav_data) {
    perror("");
    return 1;
  }

  int16_t wav_block[CHANNELS * BLOCK_FRAMES];
  bool done = false;
  while (!done) {
    done = !mix_audio(wav_block, BLOCK_FRAMES, ctx);

    if (wav_bytes == wav_bytes_cap) {
      wav_bytes_cap *= 2;
      wav_data = realloc(wav_data, wav_bytes_cap);
      if (!wav_data) {
        perror("");
        return 1;
      }
    }

    uint8_t *wav_out = wav_data + wav_bytes;
    for (size_t i = 0; i < CHANNELS * BLOCK_FRAMES; i++) {
      wav_out[0] = wav_block[i];
      wav_out[1] = wav_block[i] >> 8;
      wav_out += 2;
    }
    wav_bytes += sizeof(int16_t) * CHANNELS * BLOCK_FRAMES;
  }

  FILE *file = fopen(output, "wb");
  if (!file) {
    perror("cannot open output file");
    return 1;
  }

  const size_t master_chunk_size = 4 + 4 + 4;
  const size_t fmt_chunk_size = 4 + 4 + 2 + 2 + 4 + 4 + 2 + 2;
  const size_t sample_header_size = 4 + 4;
  const char write_err[] = "cannot write to output file";
  uint8_t buf[4];

  // Master RIFF chunk
  if (fwrite("RIFF", 1, 4, file) < 4) {
    perror(write_err);
    return 1;
  }
  size_t total_size = master_chunk_size + fmt_chunk_size + sample_header_size + wav_bytes;
  w32le(buf, total_size - 8);
  if (fwrite(buf, 1, 4, file) < 4) {
    perror(write_err);
    return 1;
  }
  if (fwrite("WAVE", 1, 4, file) < 4) {
    perror(write_err);
    return 1;
  }

  // Data format chunk
  if (fwrite("fmt ", 1, 4, file) < 4) {
    perror(write_err);
    return 1;
  }
  size_t chunk_size = fmt_chunk_size - 8;
  w32le(buf, chunk_size);
  if (fwrite(buf, 1, 4, file) < 4) {
    perror(write_err);
    return 1;
  }
  const uint16_t audio_format = 1;
  w16le(buf, audio_format);
  if (fwrite(buf, 1, 2, file) < 2) {
    perror(write_err);
    return 1;
  }
  w16le(buf, CHANNELS);
  if (fwrite(buf, 1, 2, file) < 2) {
    perror(write_err);
    return 1;
  }
  w32le(buf, SRATE);
  if (fwrite(buf, 1, 4, file) < 4) {
    perror(write_err);
    return 1;
  }
  uint32_t bytes_per_second = CHANNELS * sizeof(int16_t) * SRATE;
  w32le(buf, bytes_per_second);
  if (fwrite(buf, 1, 4, file) < 4) {
    perror(write_err);
    return 1;
  }
  uint16_t bytes_per_block = CHANNELS * sizeof(int16_t);
  w16le(buf, bytes_per_block);
  if (fwrite(buf, 1, 2, file) < 2) {
    perror(write_err);
    return 1;
  }
  uint16_t bits_per_sample = sizeof(int16_t) * 8;
  w16le(buf, bits_per_sample);
  if (fwrite(buf, 1, 2, file) < 2) {
    perror(write_err);
    return 1;
  }

  // Data chunk
  if (fwrite("data", 1, 4, file) < 4) {
    perror(write_err);
    return 1;
  }
  w32le(buf, wav_bytes);
  if (fwrite(buf, 1, 4, file) < 4) {
    perror(write_err);
    return 1;
  }
  if (fwrite(wav_data, 1, wav_bytes, file) < wav_bytes) {
    perror(write_err);
    return 1;
  }

  if (fclose(file) != 0) {
    perror(write_err);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  setlocale(LC_CTYPE, "");

  bool fade = true;
  int loops = 1;
  const char *output = 0;

  int optchar;
  while ((optchar = getopt_long(argc, argv, "hFl:o:", options, 0)) != -1) {
    switch (optchar) {
    case 'h':
      fprintf(stderr, usage, argv[0]);
      return 0;
    case 'F':
      fade = false;
      break;
    case 'l':
      loops = atoi(optarg);
      break;
    case 'o':
      output = optarg;
      break;
    default:
      fprintf(stderr, usage, argv[0]);
      return 1;
    }
  }
  if (optind + 1 != argc) {
    fprintf(stderr, usage, argv[0]);
    return 1;
  }
  char *filename = argv[optind];

  if (is_mml(filename)) {
    if (compile(&filename) != 0) return 1;
    printf("Compiled output: %s\n\n", filename);
  }

  enum fmplayer_file_error fmfile_error;
  struct fmplayer_file *fmfile = fmplayer_file_alloc(filename, &fmfile_error);
  if (!fmfile) {
    fprintf(stderr, "cannot load file: %s\n", fmplayer_file_strerror(fmfile_error));
    return 1;
  }

  struct opna opna = {0};
  struct opna_timer timer = {0};
  struct ppz8 ppz8 = {0};
  struct fmdriver_work work = {0};
  uint8_t adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  fmplayer_init_work_opna(&work, &ppz8, &opna, &timer, adpcm_ram);
  opna_ssg_set_mix(&opna.ssg, 0x10000);
  opna_ssg_set_ymf288(&opna.ssg, &opna.resampler, false);
  ppz8_set_interpolation(&ppz8, PPZ8_INTERP_SINC);
  opna_fm_set_hires_sin(&opna.fm, false);
  opna_fm_set_hires_env(&opna.fm, false);
  fmplayer_file_load(&work, fmfile, loops);

  print_comments(&work);

  struct mix_context ctx = {
    .timer = &timer,
    .work = &work,
    .volume = VOLUME_INIT,
    .loops = loops,
    .fadeout_enabled = fade,
  };

  if (output) {
    return save(output, &ctx);
  } else {
    return play(&ctx);
  }
}
