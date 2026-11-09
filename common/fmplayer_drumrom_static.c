#include "fmplayer_drumrom_static.h"

static struct {
  uint8_t *drum_rom;
} g;

void fmplayer_drum_rom_static_set(uint8_t *drum_rom) {
  g.drum_rom = drum_rom;
}

const uint8_t *fmplayer_drum_rom_get(void) {
  return g.drum_rom;
}
