#ifndef MYON_FMPLAYER_BACKEND_H_INCLUDED
#define MYON_FMPLAYER_BACKEND_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

struct ppz8;
struct fmdriver_work;

enum fmplayer_backend_id {
  FMPLAYER_BACKEND_LIBOPNA,
  FMPLAYER_BACKEND_YMFM,
};

// Opaque per-instance backend state; sized to hold the largest backend.
enum {
  FMPLAYER_BACKEND_STORAGE_SIZE = 384 * 1024,
  // ADPCM-B (Y8950) external RAM size, shared across backends
  FMPLAYER_ADPCM_RAM_SIZE = 1 << 18,
};
struct fmplayer_backend_storage {
  union {
    long double align;
    char storage[FMPLAYER_BACKEND_STORAGE_SIZE];
  };
};

// A chip emulation backend. Each backend wires a fmdriver_work and produces
// interleaved stereo int16 output via mix() at its own sample rate.
struct fmplayer_backend {
  const char *name;
  unsigned sample_rate;
  // ppz8 mix level passed to ppz8_init
  unsigned ppz8_mix;
  size_t storage_size;

  // Initialize work/ppz8/state. Drum ROM is loaded via fmplayer_drum_rom_get();
  // adpcm_ram is a caller-owned buffer of FMPLAYER_ADPCM_RAM_SIZE bytes.
  void (*init)(void *storage, struct ppz8 *ppz8, struct fmdriver_work *work,
               void *adpcm_ram);
  // Produce samples stereo frames (2*samples int16) into buf.
  void (*mix)(void *storage, int16_t *buf, unsigned samples);
};

#ifdef __cplusplus
extern "C" {
#endif

extern const struct fmplayer_backend fmplayer_backend_libopna;
extern const struct fmplayer_backend fmplayer_backend_ymfm;

#ifdef __cplusplus
}
#endif

#endif // MYON_FMPLAYER_BACKEND_H_INCLUDED
