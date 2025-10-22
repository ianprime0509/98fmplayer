#include "fmplayer_file.h"
#include <stdlib.h>
#include <string.h>

#define IMPORT(name) __attribute__((import_module("fmplayer_file"), import_name(name)))

IMPORT("size") extern size_t fmplayer_file_js_size(const char *path);
IMPORT("read") extern void fmplayer_file_js_read(const char *path, uint8_t *buf);

static void *fileread(const char *path, size_t maxsize, size_t *filesize, enum fmplayer_file_error *error) {
  uint8_t *buf = 0;
  size_t size = fmplayer_file_js_size(path);
  if (size == (size_t)-1) {
    if (error) *error = FMPLAYER_FILE_ERR_NOTFOUND;
    goto err;
  }
  if (maxsize && size > maxsize) {
    if (error) *error = FMPLAYER_FILE_ERR_BADFILE_SIZE;
    goto err;
  }
  buf = malloc(size);
  if (!buf) goto err;
  fmplayer_file_js_read(path, buf);
  *filesize = size;
  return buf;
err:
  free(buf);
  return 0;
}

void *fmplayer_fileread(const void *path, const char *pcmname, const char *extension, size_t maxsize, size_t *filesize, enum fmplayer_file_error *error) {
  if (!pcmname) return fileread(path, maxsize, filesize, error);

  char *namebuf = 0;
  char *dirbuf = 0;
  char *pcmpath = 0;
  
  if (extension) {
    size_t namebuflen = strlen(pcmname) + strlen(extension) + 1;
    namebuf = malloc(namebuflen);
    if (!namebuf) {
      if (error) *error = FMPLAYER_FILE_ERR_NOMEM;
      goto err;
    }
    strcpy(namebuf, pcmname);
    strcat(namebuf, extension);
    pcmname = namebuf;
  }

  const char *slash = strrchr(path, '/');
  const char *dirpath = 0;
  if (slash) {
    dirbuf = strdup(path);
    if (!dirbuf) {
      if (error) *error = FMPLAYER_FILE_ERR_NOMEM;
      goto err;
    }
    *strrchr(dirbuf, '/') = 0;
    dirpath = dirbuf;
  } else {
    dirpath = ".";
  }

  size_t pathlen = strlen(dirpath) + 1 + strlen(pcmname) + 1;
  pcmpath = malloc(pathlen);
  if (!pcmpath) {
    if (error) *error = FMPLAYER_FILE_ERR_NOMEM;
    goto err;
  }
  strcpy(pcmpath, dirpath);
  strcat(pcmpath, "/");
  strcat(pcmpath, pcmname);
  void *buf = fileread(pcmpath, maxsize, filesize, error);
  free(pcmpath);
  free(dirbuf);
  free(namebuf);
  return buf;

err:
  free(pcmpath);
  free(dirbuf);
  free(namebuf);
  return 0;
}

char *fmplayer_path_filename_sjis(const void *path) {
  // TODO: Shift JIS conversion
  return strdup(path);
}

void *fmplayer_path_dup(const void *path) {
  return strdup(path);
}
