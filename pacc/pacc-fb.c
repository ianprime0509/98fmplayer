#include "pacc-fb.h"
#include "pacc.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pacc_ctx {
  uint8_t *fb;
  int w;
  int h;
  uint8_t pal[256*3];
  uint8_t color;
};

struct pacc_cmd {
  int x;
  int y;
  int w;
  int h;
  int xoff;
  int yoff;
};

struct pacc_buf {
  struct pacc_cmd *buf;
  struct pacc_tex *tex;
  int len;
  int buflen;
};

struct pacc_tex {
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
    free(pb->buf);
    free(pb);
  }
}

static struct pacc_buf *pacc_gen_buf(
    struct pacc_ctx *pc, struct pacc_tex *pt, enum pacc_buf_mode mode) {
  (void)pc;
  (void)mode;
  struct pacc_buf *pb = malloc(sizeof(*pb));
  if (!pb) goto err;
  *pb = (struct pacc_buf) {
    .buflen = PACC_BUF_DEF_LEN,
    .tex = pt,
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
    struct pacc_cmd *newbuf = realloc(pb->buf, newlen * sizeof(pb->buf[0]));
    if (!newbuf) return false;
    pb->buflen = newlen;
    pb->buf = newbuf;
  }
  return true;
}

static void pacc_buf_rect_off(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, int w, int h, int xoff, int yoff) {
  (void)pc;
  if (!buf_reserve(pb, 1)) return;
  pb->buf[pb->len] = (struct pacc_cmd) {
    .x = x,
    .y = y,
    .w = w,
    .h = h,
    .xoff = xoff,
    .yoff = yoff,
  };
  pb->len += 1;
}

static void pacc_buf_rect(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, int w, int h) {
  pacc_buf_rect_off(pc, pb, x, y, w, h, 0, 0);
}

static void pacc_buf_vprintf(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, const char *fmt, va_list ap) {
  (void)pc;
  uint8_t printbuf[PRINTBUFLEN+1];
  vsnprintf((char *)printbuf, sizeof(printbuf), fmt, ap);
  int len = strlen((const char *)printbuf);
  int cw = pb->tex->w / 256;
  int ch = pb->tex->h;
  if (!buf_reserve(pb, len)) return;
  for (int i = 0; i < len; i++) {
    pb->buf[pb->len + i] = (struct pacc_cmd) {
      .x = cw*i + x,
      .y = y,
      .w = cw,
      .h = ch,
      .xoff = cw*printbuf[i],
      .yoff = 0,
    };
  }
  pb->len += len;
}

static void pacc_buf_printf(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pacc_buf_vprintf(pc, pb, x, y, fmt, ap);
  va_end(ap);
}

static void pacc_buf_clear(struct pacc_buf *pb) {
  pb->len = 0;
}

static void pacc_palette(struct pacc_ctx *pc, const uint8_t *rgb, int colors) {
  memcpy(pc->pal, rgb, colors*3);
}

static void pacc_color(struct pacc_ctx *pc, uint8_t pal) {
  pc->color = pal;
}

static void set_color(struct pacc_ctx *pc, int i, uint8_t color) {
  pc->fb[4*i+0] = pc->pal[3*color+0];
  pc->fb[4*i+1] = pc->pal[3*color+1];
  pc->fb[4*i+2] = pc->pal[3*color+2];
  pc->fb[4*i+3] = 255;
}

static void pacc_begin_clear(struct pacc_ctx *pc) {
  for (int i = 0; i < pc->w * pc->h; i++) {
    set_color(pc, i, 0);
  }
}

static void pacc_draw(struct pacc_ctx *pc, struct pacc_buf *pb, enum pacc_mode mode) {
  for (int i = 0; i < pb->len; i++) {
    struct pacc_cmd cmd = pb->buf[i];
    for (int y = 0; y < cmd.h; y++) {
      for (int x = 0; x < cmd.w; x++) {
        int xdest = cmd.x + x;
        int ydest = cmd.y + y;
        int xsrc = cmd.xoff + x;
        int ysrc = cmd.yoff + y;
        if (xdest < 0 || xsrc < 0) continue;
        if (ydest < 0 || ysrc < 0) continue;
        if (xdest >= pc->w || xsrc >= pb->tex->w) goto next_row;
        if (ydest >= pc->h || ysrc >= pb->tex->h) goto next_cmd;
        int idx = ydest*pc->w + xdest;
        int color = pb->tex->buf[ysrc*pb->tex->w + xsrc];
        switch (mode) {
        case pacc_mode_copy:
          set_color(pc, idx, color);
          break;
        case pacc_mode_color:
          set_color(pc, idx, color != 0 ? pc->color : 0);
          break;
        case pacc_mode_color_trans:
          if (color != 0) set_color(pc, idx, pc->color);
          break;
        default:
          break;
        }
      }
      next_row:;
    }
    next_cmd:;
  }
}

static uint8_t *pacc_tex_lock(struct pacc_tex *pt) {
  return pt->buf;
}

static void pacc_tex_unlock(struct pacc_tex *pt) {
  (void)pt;
}

static void pacc_tex_delete(struct pacc_tex *pt) {
  if (pt) {
    free(pt->buf);
    free(pt);
  }
}

static struct pacc_tex *pacc_gen_tex(struct pacc_ctx *pc, int w, int h) {
  (void)pc;
  struct pacc_tex *pt = malloc(sizeof(*pt));
  if (!pt) goto err;
  *pt = (struct pacc_tex) {
    .w = w,
    .h = h,
    .buf = calloc(w*h, 1),
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

static struct pacc_vtable pacc_fb_vtable = {
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

struct pacc_ctx *pacc_init_fb(uint8_t *fb, int w, int h, struct pacc_vtable *vt) {
  struct pacc_ctx *pc = malloc(sizeof(*pc));
  if (!pc) goto err;
  *pc = (struct pacc_ctx) {
    .fb = fb,
    .w = w,
    .h = h,
  };
  *vt = pacc_fb_vtable;
  return pc;
err:
  pacc_delete(pc);
  return 0;
}
