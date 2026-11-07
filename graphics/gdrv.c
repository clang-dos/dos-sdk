/* Pixel access for each memory layout: packed (CGA, MCGA 640x480,
 * Hercules), planar (EGA, VGA 16 colors) and linear (VGA 256 colors and
 * VESA, with banks).
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "graph-local.h"
#include <string.h>

/* ---- Packed: 1 or 2 bits per pixel, rows in 8 KiB banks. ---- */

static uint16_t
packed_off(int x, int y)
{
    const struct gmode *m = __g.m;
    unsigned bankmask = (1u << m->banks) - 1;
    return (uint16_t)(((unsigned)y & bankmask) * 0x2000u +
                      ((unsigned)y >> m->banks) * __g.bpl +
                      (((unsigned)x * m->bpp) >> 3));
}

static uint8_t
apply(uint8_t b, uint8_t v, uint8_t mask)
{
    switch (__g.op) {
    case OP_XOR:
        return b ^ (v & mask);
    case OP_OR:
        return b | (v & mask);
    case OP_AND:
        return b & (v | (uint8_t)~mask);
    default:
        return (b & (uint8_t)~mask) | (v & mask);
    }
}

static void
packed_plot(int x, int y, int c)
{
    gfar p = GFAR(__g.seg, packed_off(x, y));
    unsigned shift, mask;
    if (__g.m->bpp == 2) {
        shift = 6 - (((unsigned)x & 3) << 1);
        mask = 3u << shift;
        c &= 3;
    } else {
        shift = 7 - ((unsigned)x & 7);
        mask = 1u << shift;
        c &= 1;
    }
    *p = apply(*p, (uint8_t)(c << shift), (uint8_t)mask);
}

static unsigned
packed_get(int x, int y)
{
    uint8_t b = *GFAR(__g.seg, packed_off(x, y));
    if (__g.m->bpp == 2)
        return (b >> (6 - (((unsigned)x & 3) << 1))) & 3;
    return (b >> (7 - ((unsigned)x & 7))) & 1;
}

static void
packed_hline(int x1, int x2, int y, int c)
{
    unsigned ppb = 8 / __g.m->bpp; /* pixels per byte */
    uint8_t full = __g.m->bpp == 2 ? (uint8_t)((c & 3) * 0x55) : (c & 1 ? 0xff : 0);
    while (x1 <= x2 && (x1 % ppb) != 0)
        packed_plot(x1++, y, c);
    if (x1 + (int)ppb - 1 <= x2) {
        gfar p = GFAR(__g.seg, packed_off(x1, y));
        unsigned n = (unsigned)(x2 - x1 + 1) / ppb;
        if (__g.op == OP_COPY) {
            _fmemset(p, full, n);
        } else {
            for (unsigned i = 0; i < n; i++)
                p[i] = apply(p[i], full, 0xff);
        }
        x1 += n * ppb;
    }
    while (x1 <= x2)
        packed_plot(x1++, y, c);
}

/* ---- Planar: the EGA/VGA graphics controller writes all four planes. ---- */

static uint8_t gc_color = 0xff, gc_func = 0xff;

static void
planar_setup(int c)
{
    static const uint8_t func[] = { 0x00, 0x18, 0x10, 0x08 };
    if ((uint8_t)c != gc_color) {
        gc_color = (uint8_t)c;
        __g_outw(0x3ce, 0x00 | (uint16_t)(c & 15) << 8); /* set/reset */
    }
    uint8_t f = func[__g.op & 3];
    if (f != gc_func) {
        gc_func = f;
        __g_outw(0x3ce, 0x03 | (uint16_t)f << 8); /* rotate and function */
    }
}

static void
planar_plot(int x, int y, int c)
{
    planar_setup(c);
    __g_outw(0x3ce, 0x08 | (uint16_t)(0x80 >> (x & 7)) << 8); /* bit mask */
    volatile uint8_t __far *p = GFAR(__g.seg, (unsigned)y * __g.bpl + ((unsigned)x >> 3));
    (void)*p; /* load the latches */
    *p = 0xff;
}

static unsigned
planar_get(int x, int y)
{
    volatile uint8_t __far *p = GFAR(__g.seg, (unsigned)y * __g.bpl + ((unsigned)x >> 3));
    unsigned bit = 7 - ((unsigned)x & 7), c = 0;
    for (int plane = 3; plane >= 0; plane--) {
        __g_outw(0x3ce, 0x04 | (uint16_t)plane << 8); /* read map */
        c = c << 1 | ((*p >> bit) & 1);
    }
    return c;
}

static void
planar_hline(int x1, int x2, int y, int c)
{
    planar_setup(c);
    volatile uint8_t __far *p = GFAR(__g.seg, (unsigned)y * __g.bpl + ((unsigned)x1 >> 3));
    unsigned b1 = (unsigned)x1 >> 3, b2 = (unsigned)x2 >> 3;
    uint8_t lmask = 0xff >> (x1 & 7), rmask = (uint8_t)(0xff00 >> ((x2 & 7) + 1));
    if (b1 == b2) {
        __g_outw(0x3ce, 0x08 | (uint16_t)(lmask & rmask) << 8);
        (void)*p;
        *p = 0xff;
        return;
    }
    __g_outw(0x3ce, 0x08 | (uint16_t)lmask << 8);
    (void)*p;
    *p++ = 0xff;
    __g_outw(0x3ce, 0xff08);
    for (unsigned b = b1 + 1; b < b2; b++, p++) {
        (void)*p;
        *p = 0xff;
    }
    __g_outw(0x3ce, 0x08 | (uint16_t)rmask << 8);
    (void)*p;
    *p = 0xff;
}

/* ---- Linear: a byte per pixel, in 64 KiB banks for VESA. ---- */

static void
setbank(uint16_t bank)
{
    if (bank == __g.bank)
        return;
    __g.bank = bank;
    __g_int10(0x4f05, 0, 0, bank << __g.gran_shift);
}

static gfar
linear_ptr(int x, int y)
{
    uint32_t a = (uint32_t)(unsigned)y * __g.bpl + (unsigned)x;
    if (__g.m->bios >= 0x100)
        setbank((uint16_t)(a >> 16));
    return GFAR(__g.seg, (uint16_t)a);
}

static void
linear_plot(int x, int y, int c)
{
    gfar p = linear_ptr(x, y);
    *p = apply(*p, (uint8_t)c, 0xff);
}

static unsigned
linear_get(int x, int y)
{
    return *linear_ptr(x, y);
}

static void
linear_hline(int x1, int x2, int y, int c)
{
    while (x1 <= x2) {
        gfar p = linear_ptr(x1, y);
        /* The bytes up to the end of the bank. */
        unsigned room = (unsigned)(0x10000ul - (uint16_t)(uint32_t)p);
        unsigned n = (unsigned)(x2 - x1) + 1;
        if (room != 0 && n > room)
            n = room;
        if (__g.op == OP_COPY) {
            _fmemset(p, (uint8_t)c, n);
        } else {
            for (unsigned i = 0; i < n; i++)
                p[i] = apply(p[i], (uint8_t)c, 0xff);
        }
        x1 += n;
    }
}

/* ---- Dispatch. ---- */

void
__g_plot(int x, int y, int c)
{
    switch (__g.m->layout) {
    case LAYOUT_PACKED:
        packed_plot(x, y, c);
        break;
    case LAYOUT_PLANAR:
        planar_plot(x, y, c);
        break;
    default:
        linear_plot(x, y, c);
        break;
    }
}

unsigned
__g_get(int x, int y)
{
    switch (__g.m->layout) {
    case LAYOUT_PACKED:
        return packed_get(x, y);
    case LAYOUT_PLANAR:
        return planar_get(x, y);
    default:
        return linear_get(x, y);
    }
}

void
__g_hline(int x1, int x2, int y, int c)
{
    switch (__g.m->layout) {
    case LAYOUT_PACKED:
        packed_hline(x1, x2, y, c);
        break;
    case LAYOUT_PLANAR:
        planar_hline(x1, x2, y, c);
        break;
    default:
        linear_hline(x1, x2, y, c);
        break;
    }
}

void
__g_clear(void)
{
    const struct gmode *m = __g.m;
    uint8_t op = __g.op;
    __g.op = OP_COPY;
    int c = m->layout == LAYOUT_LINEAR ? __g.bkcolor : 0;
    if (m->layout == LAYOUT_PACKED) {
        /* The banks hold the whole page. */
        uint16_t size = m->banks ? (uint16_t)(0x2000u << m->banks) - 1 : (uint16_t)(__g.bpl * m->height) - 1;
        gfar p = GFAR(__g.seg, 0);
        _fmemset(p, 0, size);
        p[size] = 0;
    } else {
        for (int y = 0; y < (int)m->height; y++)
            __g_hline(0, m->width - 1, y, c);
    }
    __g.op = op;
}

void
__g_hw_begin(void)
{
    gc_color = gc_func = 0xff;
    if (__g.m->layout == LAYOUT_PLANAR) {
        __g_outw(0x3c4, 0x0f02); /* write all planes */
        __g_outw(0x3ce, 0x0f01); /* set/reset for all planes */
        __g_outw(0x3ce, 0x0005); /* write mode 0, read mode 0 */
    }
}

void
__g_hw_end(void)
{
    if (__g.m && __g.m->layout == LAYOUT_PLANAR) {
        __g_outw(0x3ce, 0x0000);
        __g_outw(0x3ce, 0x0001);
        __g_outw(0x3ce, 0x0003);
        __g_outw(0x3ce, 0xff08);
    }
}
