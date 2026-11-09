#ifndef MYON_FMPLAYER_DRUMROM_H_INCLUDED
#define MYON_FMPLAYER_DRUMROM_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// YM2608/Y8950 ADPCM-A tone ROM (drum samples), MAME format
enum {
  FMPLAYER_DRUM_ROM_SIZE = 0x2000,
};

#ifdef __cplusplus
extern "C" {
#endif

// Returns the loaded drum ROM data, or NULL if not loaded.
const uint8_t *fmplayer_drum_rom_get(void);
bool fmplayer_drum_loaded(void);

#ifdef __cplusplus
}
#endif

#endif // MYON_FMPLAYER_DRUMROM_H_INCLUDED
