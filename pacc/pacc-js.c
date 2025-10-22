#include "pacc-js.h"
#include "pacc/pacc.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMPORT(name) __attribute__((import_module("pacc"), import_name(name)))

IMPORT("init") extern void pacc_js_init(void);
IMPORT("genBuf") extern int pacc_js_gen_buf(void);
IMPORT("bufDelete") extern void pacc_js_buf_delete(int pb);
IMPORT("bufUpdate") extern void pacc_js_buf_update(int pb, const float *buf, int len, enum pacc_buf_mode mode);
IMPORT("palette") extern void pacc_js_palette(const uint8_t *rgb, int colors);
IMPORT("color") extern void pacc_js_color(uint8_t pal);
IMPORT("clear") extern void pacc_js_clear(void);
IMPORT("draw") extern void pacc_js_draw(int pt, int pb, int n, enum pacc_mode mode);
IMPORT("genTex") extern int pacc_js_gen_tex(int w, int h);
IMPORT("texDelete") extern void pacc_js_tex_delete(int pt);
IMPORT("texUpdate") extern void pacc_js_tex_update(int pt, uint8_t *buf, int w, int h);

struct pacc_ctx {
  int w;
  int h;
};

struct pacc_buf {
  int buf_obj;
  float *buf;
  struct pacc_tex *tex;
  int len;
  int buflen;
  enum pacc_buf_mode mode;
  bool changed;
};

struct pacc_tex {
  int tex_obj;
  int w;
  int h;
  uint8_t *buf;
};

enum {
  PACC_BUF_DEF_LEN = 32,
  PRINTBUFLEN = 160,
};

static void pacc_delete(struct pacc_ctx *pc) {
  free(pc);
}

static void pacc_buf_delete(struct pacc_buf *pb) {
  if (pb) {
    pacc_js_buf_delete(pb->buf_obj);
    free(pb->buf);
    free(pb);
  }
}

static struct pacc_buf *pacc_gen_buf(
    struct pacc_ctx *pc, struct pacc_tex *pt, enum pacc_buf_mode mode) {
  (void)pc;
  struct pacc_buf *pb = malloc(sizeof(*pb));
  if (!pb) goto err;
  *pb = (struct pacc_buf) {
    .buf_obj = pacc_js_gen_buf(),
    .buflen = PACC_BUF_DEF_LEN,
    .tex = pt,
    .mode = mode,
  };
  pb->buf = malloc(sizeof(*pb->buf) * pb->buflen);
  if (!pb->buf) goto err;
  return pb;
err:
  pacc_buf_delete(pb);
  return 0;
}

static bool buf_reserve(struct pacc_buf *pb, int len) {
  if (pb->len + len > pb->buflen) {
    int newlen = pb->buflen;
    while (pb->len + len > newlen) newlen *= 2;
    float *newbuf = realloc(pb->buf, newlen * sizeof(pb->buf[0]));
    if (!newbuf) return false;
    pb->buflen = newlen;
    pb->buf = newbuf;
  }
  return true;
}

static void pacc_calc_scale(float *ret, int w, int h, int wdest, int hdest) {
  ret[0] = ((float)w) / wdest;
  ret[1] = ((float)h) / hdest;
}

static void pacc_calc_off_tex(float *ret,
                                     int tw, int th, int xsrc, int ysrc) {
  ret[0] = ((float)xsrc) / tw;
  ret[1] = ((float)ysrc) / th;
}

static void pacc_calc_off(
    float *ret, int xdest, int ydest, int w, int h, int wdest, int hdest) {
  ret[0] = ((float)(xdest * 2 + w - wdest)) / wdest;
  ret[1] = ((float)(ydest * 2 + h - hdest)) / hdest;
}

static void pacc_buf_rect_off(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, int w, int h, int xoff, int yoff) {
  if (!w && !h) return;
  float scale[2];
  float off[2];
  float tscale[2];
  float toff[2];
  pacc_calc_off(off, x, y, w, h, pc->w, pc->h);
  pacc_calc_scale(scale, w, h, pc->w, pc->h);
  pacc_calc_off_tex(toff, pb->tex->w, pb->tex->h, xoff, yoff);
  pacc_calc_scale(tscale, w, h, pb->tex->w, pb->tex->h);
  float coord[16] = {
    -1.0f * scale[0] + off[0], -1.0f * scale[1] - off[1],
     0.0f * tscale[0] + toff[0],  1.0f * tscale[1] + toff[1],

    -1.0f * scale[0] + off[0],  1.0f * scale[1] - off[1],
     0.0f * tscale[0] + toff[0],  0.0f * tscale[1] + toff[1],

     1.0f * scale[0] + off[0], -1.0f * scale[1] - off[1],
     1.0f * tscale[0] + toff[0],  1.0f * tscale[1] + toff[1],

     1.0f * scale[0] + off[0],  1.0f * scale[1] - off[1],
     1.0f * tscale[0] + toff[0],  0.0f * tscale[1] + toff[1],
  };
  if (!buf_reserve(pb, 24)) return;
  int indices[6] = {0, 1, 2, 2, 1, 3};
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 4; j++) {
      pb->buf[pb->len+i*4+j] = coord[indices[i]*4+j];
    }
  }
  pb->len += 24;
  pb->changed = true;
}

static void pacc_buf_vprintf(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, const char *fmt, va_list ap) {
  uint8_t printbuf[PRINTBUFLEN+1];
  vsnprintf((char *)printbuf, sizeof(printbuf), fmt, ap);
  int len = strlen((const char *)printbuf);
  float scale[2];
  float off[2];
  int w = pb->tex->w / 256;
  int h = pb->tex->h;
  pacc_calc_scale(scale, w, h, pc->w, pc->h);
  pacc_calc_off(off, x, y, w, h, pc->w, pc->h);
  if (!buf_reserve(pb, len*24)) return;
  float *coords = pb->buf + pb->len;
  for (int i = 0; i < len; i++) {
    coords[24*i+0*4+0]                      = (-1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+0*4+1]                      = -1.0f * scale[1] - off[1];
    coords[24*i+1*4+0] = coords[24*i+4*4+0] = (-1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+1*4+1] = coords[24*i+4*4+1] = 1.0f * scale[1] - off[1];
    coords[24*i+2*4+0] = coords[24*i+3*4+0] = (1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+2*4+1] = coords[24*i+3*4+1] = -1.0f * scale[1] - off[1];
    coords[24*i+5*4+0]                      = (1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+5*4+1]                      = 1.0f * scale[1] - off[1];
    coords[24*i+0*4+2]                      = ((float)printbuf[i]) / 256.0f;
    coords[24*i+0*4+3]                      = 1.0f;
    coords[24*i+1*4+2] = coords[24*i+4*4+2] = ((float)printbuf[i]) / 256.0f;
    coords[24*i+1*4+3] = coords[24*i+4*4+3] = 0.0f;
    coords[24*i+2*4+2] = coords[24*i+3*4+2] = ((float)(printbuf[i]+1)) / 256.0f;
    coords[24*i+2*4+3] = coords[24*i+3*4+3] = 1.0f;
    coords[24*i+5*4+2]                      = ((float)(printbuf[i]+1)) / 256.0f;
    coords[24*i+5*4+3]                      = 0.0f;
  }
  pb->len += len * 24;
  pb->changed = true;
}

static void pacc_buf_printf(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pacc_buf_vprintf(pc, pb, x, y, fmt, ap);
  va_end(ap);
}

static void pacc_buf_rect(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, int w, int h) {
  pacc_buf_rect_off(pc, pb, x, y, w, h, 0, 0);
}

static void pacc_buf_clear(struct pacc_buf *pb) {
  pb->len = 0;
  pb->changed = true;
}

static void pacc_palette(struct pacc_ctx *pc, const uint8_t *rgb, int colors) {
  (void)pc;
  pacc_js_palette(rgb, colors);
}

static void pacc_color(struct pacc_ctx *pc, uint8_t pal) {
  (void)pc;
  pacc_js_color(pal);
}

static void pacc_begin_clear(struct pacc_ctx *pc) {
  (void)pc;
  pacc_js_clear();
}

static void pacc_draw(struct pacc_ctx *pc, struct pacc_buf *pb, enum pacc_mode mode) {
  (void)pc;
  if (pb->changed) {
    pacc_js_buf_update(pb->buf_obj, pb->buf, pb->len, pb->mode);
    pb->changed = false;
  }
  pacc_js_draw(pb->tex->tex_obj, pb->buf_obj, pb->len / 4, mode);
}

static uint8_t *pacc_tex_lock(struct pacc_tex *pt) {
  return pt->buf;
}

static void pacc_tex_unlock(struct pacc_tex *pt) {
  pacc_js_tex_update(pt->tex_obj, pt->buf, pt->w, pt->h);
}

static void pacc_tex_delete(struct pacc_tex *pt) {
  if (pt) {
    pacc_js_tex_delete(pt->tex_obj);
    free(pt->buf);
    free(pt);
  }
}

static struct pacc_tex *pacc_gen_tex(struct pacc_ctx *pc, int w, int h) {
  (void)pc;
  struct pacc_tex *pt = malloc(sizeof(*pt));
  if (!pt) goto err;
  *pt = (struct pacc_tex) {
    .tex_obj = pacc_js_gen_tex(w, h),
    .w = w,
    .h = h,
    .buf = malloc(w*h),
  };
  if (!pt->buf) goto err;
  return pt;
err:
  pacc_tex_delete(pt);
  return 0;
}

static void pacc_viewport_scale(struct pacc_ctx *pc, int scale) {
  (void)pc;
  (void)scale;
}

static struct pacc_vtable pacc_js_vtable = {
  .pacc_delete = pacc_delete,
  .gen_buf = pacc_gen_buf,
  .gen_tex = pacc_gen_tex,
  .buf_delete = pacc_buf_delete,
  .tex_lock = pacc_tex_lock,
  .tex_unlock = pacc_tex_unlock,
  .tex_delete = pacc_tex_delete,
  .buf_rect = pacc_buf_rect,
  .buf_rect_off = pacc_buf_rect_off,
  .buf_vprintf = pacc_buf_vprintf,
  .buf_printf = pacc_buf_printf,
  .buf_clear = pacc_buf_clear,
  .palette = pacc_palette,
  .color = pacc_color,
  .begin_clear = pacc_begin_clear,
  .draw = pacc_draw,
  .viewport_scale = pacc_viewport_scale,
};

struct pacc_ctx *pacc_init_js(int w, int h, struct pacc_vtable *vt) {
  pacc_js_init();

  struct pacc_ctx *pc = malloc(sizeof(*pc));
  if (!pc) goto err;
  *pc = (struct pacc_ctx) {
    .w = w,
    .h = h,
  };
  *vt = pacc_js_vtable;
  return pc;
err:
  pacc_delete(pc);
  return 0;
}
