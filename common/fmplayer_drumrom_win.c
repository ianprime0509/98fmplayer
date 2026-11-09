#include "fmplayer_drumrom.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <wchar.h>
static struct {
  char drum_rom[FMPLAYER_DRUM_ROM_SIZE];
  bool loaded;
} g;

static void loadrom(void) {
  const wchar_t *path = L"ym2608_adpcm_rom.bin";
  wchar_t exepath[MAX_PATH];
  if (GetModuleFileNameW(0, exepath, MAX_PATH)) {
    PathRemoveFileSpecW(exepath);
    if ((wcslen(exepath) + wcslen(path) + 1) < MAX_PATH) {
      wcscat(exepath, L"\\");
      wcscat(exepath, path);
      path = exepath;
    }
  }
  HANDLE file = CreateFileW(path, GENERIC_READ,
                            0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) goto err;
  DWORD filesize = GetFileSize(file, 0);
  if (filesize != FMPLAYER_DRUM_ROM_SIZE) goto err;
  DWORD readbytes;
  if (!ReadFile(file, g.drum_rom, FMPLAYER_DRUM_ROM_SIZE, &readbytes, 0)
      || readbytes != FMPLAYER_DRUM_ROM_SIZE) goto err;
  CloseHandle(file);
  g.loaded = true;
  return;
err:
  if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
  return;
}


const uint8_t *fmplayer_drum_rom_get(void) {
  if (!g.loaded) loadrom();
  return g.loaded ? (const uint8_t *)g.drum_rom : 0;
}

bool fmplayer_drum_loaded(void) {
  return g.loaded;
}
