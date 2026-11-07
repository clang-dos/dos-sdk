/* The graphics library: drivers and modes, starting and stopping,
 * palettes, pages and the viewport.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "graph-local.h"

struct gstate __g = { .result = grOk, .yasp = 10000 };

#define HERC 0xffff

static const struct gmode modes[] = {
    /* driver, mode, bios, w, h, colors, layout, bpp, banks, pages, pagesize, xasp, name */
    { CGA, CGAC0, 4, 320, 200, 4, LAYOUT_PACKED, 2, 1, 1, 0, 8333, "320 x 200 CGA C0" },
    { CGA, CGAC1, 4, 320, 200, 4, LAYOUT_PACKED, 2, 1, 1, 0, 8333, "320 x 200 CGA C1" },
    { CGA, CGAC2, 4, 320, 200, 4, LAYOUT_PACKED, 2, 1, 1, 0, 8333, "320 x 200 CGA C2" },
    { CGA, CGAC3, 4, 320, 200, 4, LAYOUT_PACKED, 2, 1, 1, 0, 8333, "320 x 200 CGA C3" },
    { CGA, CGAHI, 6, 640, 200, 2, LAYOUT_PACKED, 1, 1, 1, 0, 4167, "640 x 200 CGA" },
    { MCGA, MCGAC0, 4, 320, 200, 4, LAYOUT_PACKED, 2, 1, 1, 0, 8333, "320 x 200 MCGA C0" },
    { MCGA, MCGAC1, 4, 320, 200, 4, LAYOUT_PACKED, 2, 1, 1, 0, 8333, "320 x 200 MCGA C1" },
    { MCGA, MCGAC2, 4, 320, 200, 4, LAYOUT_PACKED, 2, 1, 1, 0, 8333, "320 x 200 MCGA C2" },
    { MCGA, MCGAC3, 4, 320, 200, 4, LAYOUT_PACKED, 2, 1, 1, 0, 8333, "320 x 200 MCGA C3" },
    { MCGA, MCGAMED, 6, 640, 200, 2, LAYOUT_PACKED, 1, 1, 1, 0, 4167, "640 x 200 MCGA" },
    { MCGA, MCGAHI, 0x11, 640, 480, 2, LAYOUT_PACKED, 1, 0, 1, 0, 10000, "640 x 480 MCGA" },
    { EGA, EGALO, 0x0e, 640, 200, 16, LAYOUT_PLANAR, 1, 0, 4, 0x400, 4167, "640 x 200 EGA" },
    { EGA, EGAHI, 0x10, 640, 350, 16, LAYOUT_PLANAR, 1, 0, 2, 0x800, 7292, "640 x 350 EGA" },
    { EGA64, EGA64LO, 0x0e, 640, 200, 16, LAYOUT_PLANAR, 1, 0, 1, 0x400, 4167, "640 x 200 EGA64" },
    { EGA64, EGA64HI, 0x10, 640, 350, 4, LAYOUT_PLANAR, 1, 0, 1, 0x800, 7292, "640 x 350 EGA64" },
    { HERCMONO, HERCMONOHI, HERC, 720, 348, 2, LAYOUT_PACKED, 1, 2, 2, 0x800, 6444, "720 x 348 HERCULES" },
    { VGA, VGALO, 0x0e, 640, 200, 16, LAYOUT_PLANAR, 1, 0, 4, 0x400, 4167, "640 x 200 VGA" },
    { VGA, VGAMED, 0x10, 640, 350, 16, LAYOUT_PLANAR, 1, 0, 2, 0x800, 7292, "640 x 350 VGA" },
    { VGA, VGAHI, 0x12, 640, 480, 16, LAYOUT_PLANAR, 1, 0, 1, 0, 10000, "640 x 480 VGA" },
    /* Modes that only fg_setmode uses: no BGI driver has them. */
    { 0, 0, 0x0d, 320, 200, 16, LAYOUT_PLANAR, 1, 0, 8, 0x200, 8333, "320 x 200 EGA" },
    { VGA256, VGA256LO, 0x13, 320, 200, 256, LAYOUT_LINEAR, 8, 0, 1, 0, 8333, "320 x 200 VGA 256" },
    { SVGA256, SVGA640x400, 0x100, 640, 400, 256, LAYOUT_LINEAR, 8, 0, 1, 0, 8333, "640 x 400 SVGA 256" },
    { SVGA256, SVGA640x480, 0x101, 640, 480, 256, LAYOUT_LINEAR, 8, 0, 1, 0, 10000, "640 x 480 SVGA 256" },
    { SVGA256, SVGA800x600, 0x103, 800, 600, 256, LAYOUT_LINEAR, 8, 0, 1, 0, 10000, "800 x 600 SVGA 256" },
    { SVGA256, SVGA1024x768, 0x105, 1024, 768, 256, LAYOUT_LINEAR, 8, 0, 1, 0, 10000, "1024 x 768 SVGA 256" },
};
#define NMODES (sizeof(modes) / sizeof(modes[0]))

static const char *const driver_names[] = {
    "", "CGA", "MCGA", "EGA", "EGA64", "EGAMONO", "IBM8514", "HercMono",
    "ATT400", "VGA", "PC3270", "VGA256", "SVGA256",
};

static const signed char ega_palette[16] = {
    0, 1, 2, 3, 4, 5, 20, 7, 56, 57, 58, 59, 60, 61, 62, 63,
};
/* The 200-line modes take the palette in the CGA's bits (blue, green,
 * red, then intensity in bit 4), and the BIOS sets them up so. */
static const signed char ega200_palette[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23,
};

static const signed char *
planar_palette(void)
{
    return __g.m && __g.m->height == 200 ? ega200_palette : ega_palette;
}

static const struct gmode *
find_mode(int driver, int mode)
{
    for (unsigned i = 0; i < NMODES; i++)
        if (modes[i].driver == driver && modes[i].mode == mode)
            return &modes[i];
    return 0;
}

int
graphresult(void)
{
    int r = __g.result;
    __g.result = grOk;
    return r;
}

char *
grapherrormsg(int code)
{
    switch (code) {
    case grOk:
        return "No error";
    case grNoInitGraph:
        return "(BGI) graphics not installed (use initgraph)";
    case grNotDetected:
        return "Graphics hardware not detected";
    case grFileNotFound:
        return "Device driver file not found";
    case grInvalidDriver:
        return "Invalid device driver";
    case grNoLoadMem:
        return "Not enough memory to load driver";
    case grNoScanMem:
        return "Out of memory in scan fill";
    case grNoFloodMem:
        return "Out of memory in flood fill";
    case grFontNotFound:
        return "Font file not found";
    case grNoFontMem:
        return "Not enough memory to load font";
    case grInvalidMode:
        return "Invalid graphics mode for selected driver";
    case grError:
        return "Graphics error";
    case grIOerror:
        return "Graphics I/O error";
    case grInvalidFont:
        return "Invalid font file";
    case grInvalidFontNum:
        return "Invalid font number";
    case grInvalidVersion:
        return "Invalid version number";
    default:
        return "Unknown graphics error";
    }
}

/* ---- Detection. ---- */

static int
has_vesa_mode(uint16_t mode, uint8_t *info)
{
    uint8_t __far *fp = info;
    uint16_t ax = 0x4f01, seg = (uint16_t)((uint32_t)fp >> 16), off = (uint16_t)(uint32_t)fp;
    __asm__ __volatile__("pushw %%es\n\tmovw %3, %%es\n\tpushw %%bp\n\tint $0x10\n\tpopw %%bp\n\tpopw %%es"
                         : "+a"(ax), "+c"(mode), "+D"(off)
                         : "r"(seg)
                         : "memory", "cc");
    /* Supported, and a graphics mode with window A writable. */
    return ax == 0x004f && (info[0] & 0x11) == 0x11 && (info[2] & 5) == 5;
}

int
__g_vesa_ok(uint16_t mode)
{
    static uint8_t info[256];
    return has_vesa_mode(mode, info);
}

static int
is_hercules(void)
{
    /* Bit 7 of the status port is vertical sync, which changes on a
     * Hercules card and not on an MDA. */
    uint8_t first = __g_inb(0x3ba) & 0x80;
    for (unsigned i = 0; i < 0x8000u; i++)
        if ((__g_inb(0x3ba) & 0x80) != first)
            return 1;
    return 0;
}

void
detectgraph(int *driver, int *mode)
{
    /* VGA and MCGA: the display combination code. */
    uint16_t bx = 0;
    uint16_t ax;
    __asm__ __volatile__("pushw %%bp\n\tint $0x10\n\tpopw %%bp"
                         : "=a"(ax), "+b"(bx)
                         : "a"((uint16_t)0x1a00)
                         : "cx", "dx", "si", "di", "memory", "cc");
    if ((ax & 0xff) == 0x1a) {
        uint8_t dcc = bx & 0xff;
        if (dcc == 7 || dcc == 8) {
            __g.has_vga = 1;
            *driver = VGA;
            *mode = VGAHI;
            return;
        }
        if (dcc == 0x0b || dcc == 0x0c) {
            __g.has_vga = 1;
            *driver = MCGA;
            *mode = MCGAHI;
            return;
        }
    }
    /* EGA: INT 10h function 12h reports its memory and monitor. */
    uint16_t ebx = 0x10, ecx = 0;
    __asm__ __volatile__("pushw %%bp\n\tint $0x10\n\tpopw %%bp"
                         : "+b"(ebx), "+c"(ecx)
                         : "a"((uint16_t)0x1200)
                         : "dx", "si", "di", "memory", "cc");
    if ((ebx & 0xff) != 0x10) {
        if (ebx >> 8) {
            *driver = EGAMONO;
            *mode = EGAMONOHI;
        } else if ((ebx & 0xff) == 0) {
            *driver = EGA64;
            *mode = EGA64HI;
        } else {
            *driver = EGA;
            *mode = EGAHI;
        }
        return;
    }
    /* CGA, or a monochrome adapter. */
    uint16_t equip = *(volatile uint16_t __far *)GFAR(0x40, 0x10);
    if ((equip & 0x30) == 0x30) {
        if (is_hercules()) {
            *driver = HERCMONO;
            *mode = HERCMONOHI;
            return;
        }
        *driver = grNotDetected;
        __g.result = grNotDetected;
        return;
    }
    *driver = CGA;
    *mode = CGAHI;
}

/* ---- Modes. ---- */

static void
herc_mode(int graphics)
{
    static const uint8_t gfx[12] = { 0x35, 0x2d, 0x2e, 0x07, 0x5b, 0x02,
                                     0x57, 0x57, 0x02, 0x03, 0x00, 0x00 };
    static const uint8_t txt[12] = { 0x61, 0x50, 0x52, 0x0f, 0x19, 0x06,
                                     0x19, 0x19, 0x02, 0x0d, 0x0b, 0x0c };
    const uint8_t *r = graphics ? gfx : txt;
    __g_outb(0x3bf, graphics ? 3 : 0); /* allow graphics and page 1 */
    __g_outb(0x3b8, graphics ? 0x02 : 0x20); /* display off while changing */
    for (int i = 0; i < 12; i++)
        __g_outw(0x3b4, (uint16_t)(i | r[i] << 8));
    __g_outb(0x3b8, graphics ? 0x0a : 0x28);
}

static int
set_mode(const struct gmode *m)
{
    __g.bank = 0xffff;
    __g.seg = 0xa000;
    if (m->bios == HERC) {
        herc_mode(1);
        __g.seg = 0xb000;
        __g.bpl = 90;
        /* The BIOS mode stays 7; text output must not scroll the screen. */
        return 1;
    }
    if (m->bios >= 0x100) {
        static uint8_t info[256];
        if (!has_vesa_mode(m->bios, info))
            return 0;
        uint16_t gran = info[4] | info[5] << 8; /* KiB */
        __g.gran_shift = 0;
        while (gran && (64u >> __g.gran_shift) > gran)
            __g.gran_shift++;
        __g.winseg = info[8] | info[9] << 8;
        if (__g.winseg == 0)
            __g.winseg = 0xa000;
        __g.seg = __g.winseg;
        __g.bpl = info[16] | info[17] << 8;
        if (__g.bpl == 0)
            __g.bpl = m->width;
        if (__g_int10(0x4f02, m->bios, 0, 0) != 0x004f)
            return 0;
        return 1;
    }
    __g_int10(m->bios, 0, 0, 0);
    if ((*(volatile uint8_t __far *)GFAR(0x40, 0x49) & 0x7f) != m->bios)
        return 0;
    if (m->layout == LAYOUT_PACKED) {
        __g.seg = m->bios == 0x11 ? 0xa000 : 0xb800;
        __g.bpl = 80;
    } else if (m->layout == LAYOUT_PLANAR) {
        __g.bpl = m->width / 8;
    } else {
        __g.bpl = m->width;
    }
    return 1;
}

static void
text_mode(void)
{
    __g_hw_end();
    if (__g.m && __g.m->bios == HERC)
        herc_mode(0);
    __g_int10(__g.orig_mode, 0, 0, 0);
}

static int enter(const struct gmode *m);
static void load_font(void);

const struct gmode *
__g_find_bios(uint16_t bios)
{
    for (unsigned i = 0; i < NMODES; i++)
        if (modes[i].bios == bios)
            return &modes[i];
    return 0;
}

int
__g_enter(const struct gmode *m)
{
    if (!__g.active)
        __g.orig_mode = *(volatile uint8_t __far *)GFAR(0x40, 0x49) & 0x7f;
    if (!__g.font_lo)
        load_font();
    if (m->driver == VGA || m->driver == VGA256 || m->driver == SVGA256 || m->bios == 0x11)
        __g.has_vga = 1;
    return enter(m);
}

static int
enter(const struct gmode *m)
{
    if (!set_mode(m)) {
        if (__g.active)
            text_mode();
        __g.active = 0;
        return 0;
    }
    __g.m = m;
    __g.driver = m->driver;
    __g.mode = m->mode;
    __g.maxx = m->width - 1;
    __g.maxy = m->height - 1;
    __g.maxcolor = m->colors - 1;
    __g.xasp = m->xasp;
    __g.yasp = 10000;
    __g.active = 1;
    __g_hw_begin();
    graphdefaults();
    __g_clear();
    return 1;
}

/* A font looks right when its space is blank and its A is not. */
static int
font_ok(const uint8_t __far *f)
{
    uint8_t space = 0, a = 0;
    for (int i = 0; i < 8; i++) {
        space |= f[' ' * 8 + i];
        a |= f['A' * 8 + i];
    }
    return space == 0 && a != 0;
}

/* The 8x8 font: from the EGA/VGA BIOS (INT 10h function 1130h), or, when
 * that gives none, the first half from F000:FA6E, where the PC BIOS keeps
 * it for the CGA, and the second from the INT 1Fh vector. */
static void
load_font(void)
{
    const uint8_t __far *half[2];
    for (int h = 0; h < 2; h++) {
        uint16_t ax = 0x1130, bx = h ? 0x0400 : 0x0300;
        __asm__ __volatile__("pushw %%es\n\tpushw %%bp\n\tint $0x10\n\tmovw %%bp, %%bx\n\t"
                             "movw %%es, %%ax\n\tpopw %%bp\n\tpopw %%es"
                             : "+a"(ax), "+b"(bx)
                             :
                             : "cx", "dx", "si", "di", "memory", "cc");
        half[h] = (const uint8_t __far *)GFAR(ax, bx);
    }
    if (font_ok(half[0])) {
        __g.font_lo = half[0];
        __g.font_hi = half[1];
        return;
    }
    __g.font_lo = (const uint8_t __far *)GFAR(0xf000, 0xfa6e);
    const uint16_t __far *vec = (const uint16_t __far *)GFAR(0, 0x1f * 4);
    __g.font_hi = vec[0] || vec[1] ? (const uint8_t __far *)GFAR(vec[1], vec[0]) : 0;
}

void
initgraph(int *driver, int *mode, const char *path)
{
    (void)path;
    if (*driver == DETECT) {
        detectgraph(driver, mode);
        if (*driver < 0) {
            __g.result = *driver;
            return;
        }
    } else if (*driver == VGA || *driver == VGA256 || *driver == SVGA256 || *driver == MCGA) {
        __g.has_vga = 1;
    }
    const struct gmode *m = find_mode(*driver, *mode);
    if (!m) {
        int known = 0;
        for (unsigned i = 0; i < NMODES; i++)
            known |= modes[i].driver == *driver;
        __g.result = known ? grInvalidMode : grInvalidDriver;
        *driver = __g.result;
        return;
    }
    if (!__g.active)
        __g.orig_mode = *(volatile uint8_t __far *)GFAR(0x40, 0x49) & 0x7f;
    load_font();
    if (!enter(m)) {
        __g.result = grInvalidMode;
        *driver = grInvalidMode;
        return;
    }
    __g.result = grOk;
}

void
closegraph(void)
{
    if (__g.active)
        text_mode();
    __g.active = 0;
}

void
restorecrtmode(void)
{
    closegraph();
}

void
setgraphmode(int mode)
{
    const struct gmode *m = find_mode(__g.driver, mode);
    if (!m) {
        __g.result = grInvalidMode;
        return;
    }
    if (!enter(m))
        __g.result = grInvalidMode;
}

int
getgraphmode(void)
{
    return __g.mode;
}

void
getmoderange(int driver, int *lo, int *hi)
{
    if (driver == CURRENT_DRIVER)
        driver = __g.driver;
    *lo = *hi = -1;
    for (unsigned i = 0; i < NMODES; i++) {
        if (modes[i].driver != driver)
            continue;
        if (*lo < 0 || modes[i].mode < *lo)
            *lo = modes[i].mode;
        if (modes[i].mode > *hi)
            *hi = modes[i].mode;
    }
    if (*lo < 0)
        __g.result = grInvalidDriver;
}

int
getmaxmode(void)
{
    int lo, hi;
    getmoderange(__g.driver, &lo, &hi);
    return hi;
}

char *
getmodename(int mode)
{
    const struct gmode *m = find_mode(__g.driver, mode);
    return (char *)(m ? m->name : "");
}

char *
getdrivername(void)
{
    if (__g.driver > 0 && __g.driver < (int)(sizeof(driver_names) / sizeof(driver_names[0])))
        return (char *)driver_names[__g.driver];
    return "";
}

unsigned
setgraphbufsize(unsigned size)
{
    (void)size;
    return 4096;
}

int
installuserdriver(const char *name, int (*detect)(void))
{
    (void)name;
    (void)detect;
    return grError;
}

int
installuserfont(const char *name)
{
    (void)name;
    return DEFAULT_FONT;
}

int
registerbgidriver(void (*driver)(void))
{
    (void)driver;
    return 0;
}

int
registerbgifont(void (*font)(void))
{
    (void)font;
    return 0;
}

void EGAVGA_driver(void) {}
void CGA_driver(void) {}
void Herc_driver(void) {}
void ATT_driver(void) {}
void PC3270_driver(void) {}
void IBM8514_driver(void) {}
void triplex_font(void) {}
void small_font(void) {}
void sansserif_font(void) {}
void gothic_font(void) {}

/* ---- Defaults. ---- */

void
graphdefaults(void)
{
    setviewport(0, 0, __g.maxx, __g.maxy, CLIP_ON);
    __g.writemode = COPY_PUT;
    __g.op = OP_COPY;
    __g.bkcolor = 0;
    setallpalette(getdefaultpalette());
    if (__g.m->colors == 4 || __g.m->colors == 2)
        setbkcolor(0);
    __g.color = __g.maxcolor > 15 ? 15 : __g.maxcolor;
    setlinestyle(SOLID_LINE, 0xffff, NORM_WIDTH);
    for (int i = 0; i < 8; i++)
        __g.userpat[i] = 0xff;
    setfillstyle(SOLID_FILL, __g.color);
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
    settextjustify(LEFT_TEXT, TOP_TEXT);
}

/* ---- The screen. ---- */

int
getmaxx(void)
{
    return __g.maxx;
}

int
getmaxy(void)
{
    return __g.maxy;
}

int
getmaxcolor(void)
{
    return __g.maxcolor;
}

void
getaspectratio(int *xasp, int *yasp)
{
    *xasp = __g.xasp;
    *yasp = __g.yasp;
}

void
setaspectratio(int xasp, int yasp)
{
    __g.xasp = xasp;
    __g.yasp = yasp;
}

void
cleardevice(void)
{
    __g_clear();
    __g.cpx = __g.cpy = 0;
}

void
setactivepage(int page)
{
    if (page < 0 || page >= __g.m->pages)
        return;
    __g.seg = (__g.m->bios == HERC ? 0xb000 : 0xa000) + page * __g.m->pagesize;
}

void
setvisualpage(int page)
{
    if (page < 0 || page >= __g.m->pages)
        return;
    if (__g.m->bios == HERC)
        __g_outb(0x3b8, page ? 0x8a : 0x0a);
    else
        __g_int10(0x0500 | page, 0, 0, 0);
}

/* ---- The viewport. ---- */

void
setviewport(int left, int top, int right, int bottom, int clip)
{
    if (left < 0 || top < 0 || right > __g.maxx || bottom > __g.maxy ||
        left > right || top > bottom) {
        __g.result = grError;
        return;
    }
    __g.vl = left;
    __g.vt = top;
    __g.vr = right;
    __g.vb = bottom;
    __g.clip = clip;
    __g.cl = clip ? left : 0;
    __g.ct = clip ? top : 0;
    __g.cr = clip ? right : __g.maxx;
    __g.cb = clip ? bottom : __g.maxy;
    __g.cpx = __g.cpy = 0;
}

void
getviewsettings(struct viewporttype *vp)
{
    vp->left = __g.vl;
    vp->top = __g.vt;
    vp->right = __g.vr;
    vp->bottom = __g.vb;
    vp->clip = __g.clip;
}

void
clearviewport(void)
{
    int c = __g.m->layout == LAYOUT_LINEAR ? __g.bkcolor : 0;
    uint8_t op = __g.op;
    __g.op = OP_COPY;
    for (int y = __g.vt; y <= __g.vb; y++)
        __g_hline(__g.vl, __g.vr, y, c);
    __g.op = op;
    __g.cpx = __g.cpy = 0;
}

/* ---- Colors and palettes. ---- */

void
setcolor(int color)
{
    __g.color = color & __g.maxcolor;
}

int
getcolor(void)
{
    return __g.color;
}

static void
cga_background(int color)
{
    /* Bit 4: the bright palette, for CGAC2 and CGAC3. */
    int bright = __g.m->colors == 4 && (__g.mode == CGAC2 || __g.mode == CGAC3);
    __g_int10(0x0b00, (uint16_t)((color & 15) | (bright ? 0x10 : 0)), 0, 0);
}

void
setbkcolor(int color)
{
    __g.bkcolor = color;
    const struct gmode *m = __g.m;
    if (!m)
        return;
    if (m->layout == LAYOUT_PLANAR) {
        __g.bkcolor = color & 15;
        setpalette(0, planar_palette()[color & 15]);
    } else if (m->colors == 4 && m->bios != HERC) {
        cga_background(color);
    }
}

int
getbkcolor(void)
{
    return __g.bkcolor;
}

struct palettetype *
getdefaultpalette(void)
{
    static struct palettetype def;
    const struct gmode *m = __g.m;
    int n = m && m->colors < 16 ? m->colors : 16;
    def.size = (unsigned char)n;
    for (int i = 0; i < 16; i++)
        def.colors[i] = (m && m->layout == LAYOUT_PLANAR) ? planar_palette()[i] : (signed char)(i < n ? i : -1);
    if (m && m->colors == 2)
        def.colors[1] = 15;
    return &def;
}

int
getpalettesize(void)
{
    return getdefaultpalette()->size;
}

void
getpalette(struct palettetype *p)
{
    *p = __g.pal;
}

void
setpalette(int index, int color)
{
    if (index < 0 || index >= __g.pal.size) {
        __g.result = grError;
        return;
    }
    __g.pal.colors[index] = (signed char)color;
    const struct gmode *m = __g.m;
    if (m->layout == LAYOUT_PLANAR) {
        __g_int10(0x1000, (uint16_t)(index | (color & 63) << 8), 0, 0);
    } else if (m->colors == 4 && index == 0) {
        cga_background(color);
    }
}

void
setallpalette(const struct palettetype *p)
{
    struct palettetype def = *getdefaultpalette();
    __g.pal.size = def.size;
    for (int i = 0; i < def.size && i < (int)p->size; i++) {
        if (p->colors[i] < 0)
            continue;
        if (__g.m->layout == LAYOUT_PLANAR)
            setpalette(i, p->colors[i]);
        else
            __g.pal.colors[i] = p->colors[i];
    }
    if (__g.m->layout == LAYOUT_PACKED && __g.m->colors == 4 && __g.m->bios == 4) {
        /* The CGA palettes of the C0-C3 modes. */
        __g_int10(0x0b00, 0x0100 | (__g.mode & 1), 0, 0);
        cga_background(__g.bkcolor);
    }
}

void
setrgbpalette(int index, int red, int green, int blue)
{
    if (!__g.has_vga)
        return;
    __g_outb(0x3c8, (uint8_t)index);
    __g_outb(0x3c9, (uint8_t)((red & 0xff) >> 2));
    __g_outb(0x3c9, (uint8_t)((green & 0xff) >> 2));
    __g_outb(0x3c9, (uint8_t)((blue & 0xff) >> 2));
}
