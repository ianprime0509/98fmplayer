#ifndef MYON_PACC_FB_H_INCLUDED
#define MYON_PACC_FB_H_INCLUDED

#include "pacc.h"

struct pacc_ctx *pacc_init_fb(uint8_t *fb, int w, int h, struct pacc_vtable *vt);

#endif // MYON_PACC_FB_H_INCLUDED
