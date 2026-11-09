#include "fmplayer_drumrom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
  uint8_t drum_rom[FMPLAYER_DRUM_ROM_SIZE];
  bool loaded;
} g;

#define DATADIR "/.local/share/98fmplayer/"

static void loadfile(void) {
  const char *path = "ym2608_adpcm_rom.bin";
  const char *home = getenv("HOME");
  char *dpath = 0;
  if (home) {
    const char *datadir = DATADIR;
    dpath = malloc(strlen(home)+strlen(datadir)+strlen(path) + 1);
    if (dpath) {
      strcpy(dpath, home);
      strcat(dpath, datadir);
      strcat(dpath, path);
      path = dpath;
    }
  }
  FILE *rhythm = fopen(path, "r");
  free(dpath);
  if (!rhythm) goto err;
  if (fseek(rhythm, 0, SEEK_END) != 0) goto err_file;
  long size = ftell(rhythm);
  if (size != FMPLAYER_DRUM_ROM_SIZE) goto err_file;
  if (fseek(rhythm, 0, SEEK_SET) != 0) goto err_file;
  if (fread(g.drum_rom, 1, FMPLAYER_DRUM_ROM_SIZE, rhythm) != FMPLAYER_DRUM_ROM_SIZE) goto err_file;
  fclose(rhythm);
  g.loaded = true;
  return;
err_file:
  fclose(rhythm);
err:
  return;
}

const uint8_t *fmplayer_drum_rom_get(void) {
  if (!g.loaded) {
    loadfile();
  }
  return g.loaded ? g.drum_rom : 0;
}

bool fmplayer_drum_loaded(void) {
  return g.loaded;
}
