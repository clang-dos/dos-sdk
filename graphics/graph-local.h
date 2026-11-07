/* The state of the graphics library, shared by its source files.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef GRAPH_LOCAL_H
#define GRAPH_LOCAL_H

#include "graphics.h"
#include <stdint.h>

typedef uint8_t __far *gfar;
#define GFAR(seg, off) ((gfar)(((uint32_t)(uint16_t)(seg) << 16) | (uint16_t)(off)))

/* How a mode lays out its pixels. */
enum {
    LAYOUT_PACKED, /* 1 or 2 bits per pixel, rows interleaved in banks of 8 KiB (CGA, Hercules) */
    LAYOUT_PLANAR, /* 4 planes of 1 bit per pixel (EGA, VGA) */
    LAYOUT_LINEAR, /* a byte per pixel (VGA 256 colors, VESA) */
};

struct gmode {
    uint8_t driver, mode;
    uint16_t bios;     /* BIOS (or VESA) mode number; 0xffff: Hercules */
    uint16_t width, height;
    uint16_t colors;
    uint8_t layout, bpp;
    uint8_t banks;     /* LAYOUT_PACKED: rows are interleaved over 1 << banks banks */
    uint8_t pages;
    uint16_t pagesize; /* paragraphs from one page to the next */
    uint16_t xasp;     /* pixel aspect, against 10000 */
    const char *name;
};

/* The pixel operations of setwritemode and putimage. */
enum { OP_COPY, OP_XOR, OP_OR, OP_AND, OP_NOT };

struct gstate {
    const struct gmode *m;
    int result;
    uint8_t active;     /* in graphics mode */
    uint8_t orig_mode;  /* the text mode before initgraph */
    uint8_t has_vga;
    int driver, mode;
    uint16_t seg;       /* segment of the active page */
    uint16_t bpl;       /* bytes per row */
    int maxx, maxy, maxcolor;
    int color, bkcolor;
    int fillstyle, fillcolor;
    uint8_t fillpat[8];
    uint8_t userpat[8];
    int linestyle;
    uint16_t linepat;
    int thickness;
    int writemode;
    int vl, vt, vr, vb, clip;
    int cl, ct, cr, cb; /* the clip rectangle: the viewport or the screen */
    int cpx, cpy;
    int font, dir, charsize, hjust, vjust;
    int usermx, userdx, usermy, userdy; /* setusercharsize */
    int xasp, yasp;
    struct arccoordstype arc;
    struct palettetype pal;
    uint8_t op;         /* the pixel operation for the next drawing */
    /* VESA banks. */
    uint16_t bank;      /* the mapped bank */
    uint8_t gran_shift; /* bank number = 64 KiB window << gran_shift */
    uint16_t winseg;
    /* The 8x8 font of the BIOS: characters 0-127 and 128-255. */
    const uint8_t __far *font_lo, *font_hi;
};

extern struct gstate __g;

/* The mode with this BIOS (or VESA) number, and entering a mode for
 * fg_setmode. */
const struct gmode *__g_find_bios(uint16_t bios);
int __g_enter(const struct gmode *m);
/* Is this VESA mode there? */
int __g_vesa_ok(uint16_t mode);

/* Pixel access for the current mode, with absolute coordinates that are
 * on the screen. hline fills x1..x2 (x1 <= x2) with the solid color c;
 * all use the operation __g.op. */
void __g_plot(int x, int y, int c);
unsigned __g_get(int x, int y);
void __g_hline(int x1, int x2, int y, int c);
/* Clear the active page to color 0. */
void __g_clear(void);
/* Set up the hardware for drawing after a mode set, and back. */
void __g_hw_begin(void);
void __g_hw_end(void);

/* Drawing in viewport coordinates, clipped. */
void __g_pixel(int x, int y, int c);
/* A span of color c, in viewport coordinates, clipped. */
void __g_span(int x1, int x2, int y, int c);
/* A span of the fill pattern and color, in viewport coordinates. */
void __g_fillspan(int x1, int x2, int y);
/* A line in the line style and color. */
void __g_line(int x1, int y1, int x2, int y2);

/* Sine and cosine of a whole number of degrees, times 16384. */
int __g_sin(int deg);
int __g_cos(int deg);

/* Video BIOS: INT 10h with AX and BX, returning AX. */
static __inline__ uint16_t
__g_int10(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx)
{
    __asm__ __volatile__("pushw %%bp\n\tint $0x10\n\tpopw %%bp"
                         : "+a"(ax), "+b"(bx), "+c"(cx), "+d"(dx)
                         :
                         : "si", "di", "memory", "cc");
    return ax;
}

static __inline__ void
__g_outb(uint16_t port, uint8_t v)
{
    __asm__ __volatile__("outb %b0, %w1" : : "a"(v), "Nd"(port));
}

static __inline__ void
__g_outw(uint16_t port, uint16_t v)
{
    __asm__ __volatile__("outw %w0, %w1" : : "a"(v), "Nd"(port));
}

static __inline__ uint8_t
__g_inb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %w1, %b0" : "=a"(v) : "Nd"(port));
    return v;
}

#endif
