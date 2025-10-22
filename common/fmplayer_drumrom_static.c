#include "fmplayer_drumrom_static.h"
#include "libopna/opnadrum.h"

static struct {
  uint8_t *drum_rom;
} g;

void fmplayer_drum_rom_static_set(uint8_t *drum_rom) {
  g.drum_rom = drum_rom;
}

bool fmplayer_drum_rom_load(struct opna_drum *drum) {
  opna_drum_set_rom(drum, g.drum_rom);
  return true;
}
