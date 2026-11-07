/* Images: copies of screen rectangles, put back with an operation.
 *
 * An image is two 16-bit words, the width and the height, then the rows
 * of pixels, each packed into whole bytes: 1, 2, 4 or 8 bits a pixel,
 * as the mode has 2, 4, 16 or 256 colors.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "graph-local.h"
#include <stdlib.h>

static unsigned
image_bpp(void)
{
    const struct gmode *m = __g.m;
    return m->layout == LAYOUT_PLANAR ? 4 : m->bpp;
}

unsigned
imagesize(int left, int top, int right, int bottom)
{
    uint32_t w = (uint32_t)abs(right - left) + 1, h = (uint32_t)abs(bottom - top) + 1;
    uint32_t size = 4 + (w * image_bpp() + 7) / 8 * h;
    return size > 0xffff ? 0xffff : (unsigned)size;
}

void
getimage(int left, int top, int right, int bottom, void *bitmap)
{
    if (left > right) {
        int t = left;
        left = right;
        right = t;
    }
    if (top > bottom) {
        int t = top;
        top = bottom;
        bottom = t;
    }
    uint16_t *hdr = bitmap;
    unsigned w = right - left + 1, h = bottom - top + 1, bpp = image_bpp();
    hdr[0] = (uint16_t)w;
    hdr[1] = (uint16_t)h;
    uint8_t *p = (uint8_t *)bitmap + 4;
    unsigned rowbytes = (w * bpp + 7) / 8;
    for (unsigned y = 0; y < h; y++, p += rowbytes) {
        int sy = top + (int)y + __g.vt;
        for (unsigned i = 0; i < rowbytes; i++)
            p[i] = 0;
        if (sy < 0 || sy > __g.maxy)
            continue;
        for (unsigned x = 0; x < w; x++) {
            int sx = left + (int)x + __g.vl;
            if (sx < 0 || sx > __g.maxx)
                continue;
            unsigned c = __g_get(sx, sy);
            unsigned bit = x * bpp;
            p[bit >> 3] |= (uint8_t)(c << (8 - bpp - (bit & 7)));
        }
    }
}

void
putimage(int left, int top, const void *bitmap, int op)
{
    const uint16_t *hdr = bitmap;
    unsigned w = hdr[0], h = hdr[1], bpp = image_bpp();
    const uint8_t *p = (const uint8_t *)bitmap + 4;
    unsigned rowbytes = (w * bpp + 7) / 8, mask = (1u << bpp) - 1;
    uint8_t saved = __g.op;
    switch (op) {
    case XOR_PUT:
        __g.op = OP_XOR;
        break;
    case OR_PUT:
        __g.op = OP_OR;
        break;
    case AND_PUT:
        __g.op = OP_AND;
        break;
    default:
        __g.op = OP_COPY;
        break;
    }
    for (unsigned y = 0; y < h; y++, p += rowbytes) {
        for (unsigned x = 0; x < w; x++) {
            unsigned bit = x * bpp;
            unsigned c = (p[bit >> 3] >> (8 - bpp - (bit & 7))) & mask;
            if (op == NOT_PUT)
                c = ~c & mask;
            __g_pixel(left + (int)x, top + (int)y, (int)c);
        }
    }
    __g.op = saved;
}
