/* The fg_ functions of <fastgraf.h>, on the drivers of <graphics.h>.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "fastgraf.h"
#include "graph-local.h"
#include <string.h>

static int fg_mode = -1;     /* -1: not set yet, the mode at startup */
static int fg_page, fg_vpage;
static int text_row, text_col;
static int just_x = -1, just_y = -1;
static int box_dx = 1, box_dy = 1;
static int adapter = -1;     /* the BGI driver detectgraph finds */

static volatile uint8_t __far *
bda(unsigned off)
{
    return (volatile uint8_t __far *)GFAR(0x40, off);
}

static uint32_t
ticks(void)
{
    return *(volatile uint32_t __far *)GFAR(0x40, 0x6c);
}

/* The BIOS mode (or VESA mode) of a Fastgraph mode, or 0. */
static uint16_t
bios_of(int mode)
{
    switch (mode) {
    case 4: case 5: return 4;
    case 6: return 6;
    case 11: return 0xffff;
    case 13: return 0x0d;
    case 14: return 0x0e;
    case 16: return 0x10;
    case 17: return 0x11;
    case 18: return 0x12;
    case 19: return 0x13;
    case 24: return 0x100;
    case 25: return 0x101;
    case 26: return 0x103;
    case 27: return 0x105;
    }
    return 0;
}

static int
is_text(int mode)
{
    return (mode >= 0 && mode <= 3) || mode == 7;
}

static int
detect(void)
{
    if (adapter < 0) {
        int d, m;
        int saved = __g.result;
        detectgraph(&d, &m);
        __g.result = saved;
        adapter = d;
    }
    return adapter;
}

int
fg_testmode(int mode, int pages)
{
    if (pages < 1)
        pages = 1;
    if (is_text(mode))
        return pages == 1 || mode != 7;
    uint16_t b = bios_of(mode);
    const struct gmode *m = b ? __g_find_bios(b) : 0;
    if (!m || m->pages < pages)
        return 0;
    int a = detect();
    switch (mode) {
    case 4: case 5: case 6:
        return a == CGA || a == MCGA || a == EGA || a == EGA64 || a == VGA;
    case 11:
        return a == HERCMONO;
    case 13: case 14: case 16:
        return a == EGA || a == EGA64 || a == VGA;
    case 17: case 19:
        return a == VGA || a == MCGA;
    case 18:
        return a == VGA;
    default:
        return a == VGA && __g_vesa_ok(b);
    }
}

int
fg_bestmode(int w, int h, int pages)
{
    static const int order[] = { 19, 13, 4, 6, 14, 16, 18, 17, 24, 25, 26, 27, 11 };
    for (unsigned i = 0; i < sizeof order / sizeof *order; i++) {
        uint16_t b = bios_of(order[i]);
        const struct gmode *m = __g_find_bios(b);
        if (m && m->width >= w && m->height >= h && fg_testmode(order[i], pages))
            return order[i];
    }
    return -1;
}

int
fg_automode(void)
{
    switch (detect()) {
    case VGA: return 18;
    case MCGA: return 19;
    case EGA: case EGA64: return 16;
    case CGA: return 4;
    case HERCMONO: return 11;
    }
    return 3;
}

int
fg_setmode(int mode)
{
    if (mode < 0) {
        fg_reset();
        return 0;
    }
    if (is_text(mode)) {
        if (__g.active)
            closegraph();
        __g_int10((uint16_t)mode, 0, 0, 0);
        fg_mode = mode;
        text_row = text_col = 0;
        return 0;
    }
    uint16_t b = bios_of(mode);
    const struct gmode *m = b ? __g_find_bios(b) : 0;
    if (!m || !__g_enter(m))
        return 0;
    fg_mode = mode;
    fg_page = fg_vpage = 0;
    text_row = text_col = 0;
    just_x = just_y = -1;
    box_dx = box_dy = 1;
    __g.color = 0;
    return 0;
}

int
fg_getmode(void)
{
    return fg_mode < 0 ? (*bda(0x49) & 0x7f) : fg_mode;
}

void
fg_reset(void)
{
    if (__g.active)
        closegraph();
    fg_mode = -1;
}

int
fg_getmaxx(void)
{
    return is_text(fg_getmode()) ? *bda(0x4a) - 1 : __g.maxx;
}

int
fg_getmaxy(void)
{
    return is_text(fg_getmode()) ? 24 : __g.maxy;
}

int
fg_colors(void)
{
    return is_text(fg_getmode()) ? 16 : __g.maxcolor + 1;
}

int
fg_svgainit(int method)
{
    (void)method;
    return __g_vesa_ok(0x101) || __g_vesa_ok(0x100) ? 1 : 0;
}

int
fg_svgastat(void)
{
    return fg_svgainit(0);
}

void
fg_version(int *major, int *minor)
{
    *major = 1;
    *minor = 0;
}

void
fg_initpm(void)
{
}

/* ---- Colors. ---- */

void
fg_setcolor(int color)
{
    __g.color = color & (__g.maxcolor ? __g.maxcolor : 255);
}

int
fg_getcolor(void)
{
    return __g.color;
}

void
fg_palette(int number, int color)
{
    if (!__g.active)
        return;
    if (__g.m->layout == LAYOUT_PLANAR)
        __g_int10(0x1000, (uint16_t)((number & 15) | (color & 63) << 8), 0, 0);
    else if (__g.m->colors == 4 && __g.m->bios == 4) {
        if (number == 0) /* the background */
            __g_int10(0x0b00, (uint16_t)(color & 15), 0, 0);
        else             /* the palette */
            __g_int10(0x0b00, 0x0100 | (color & 1), 0, 0);
    }
}

void
fg_setrgb(int number, int r, int g, int b)
{
    if (__g.active && __g.m->layout == LAYOUT_PLANAR && number < 16) {
        /* Point palette register number at DAC register number. */
        __g_int10(0x1000, (uint16_t)(number | number << 8), 0, 0);
    }
    __g_outb(0x3c8, (uint8_t)number);
    __g_outb(0x3c9, (uint8_t)(r & 63));
    __g_outb(0x3c9, (uint8_t)(g & 63));
    __g_outb(0x3c9, (uint8_t)(b & 63));
}

void
fg_getrgb(int number, int *r, int *g, int *b)
{
    __g_outb(0x3c7, (uint8_t)number);
    *r = __g_inb(0x3c9) & 63;
    *g = __g_inb(0x3c9) & 63;
    *b = __g_inb(0x3c9) & 63;
}

void
fg_setdacs(int start, int count, const char *v)
{
    __g_outb(0x3c8, (uint8_t)start);
    for (int i = 0; i < count * 3; i++)
        __g_outb(0x3c9, (uint8_t)(v[i] & 63));
}

void
fg_getdacs(int start, int count, char *v)
{
    __g_outb(0x3c7, (uint8_t)start);
    for (int i = 0; i < count * 3; i++)
        v[i] = (char)(__g_inb(0x3c9) & 63);
}

static uint8_t virtual_colors[256];
static uint8_t virtual_init;

void
fg_defcolor(int index, int value)
{
    if (!virtual_init) {
        for (int i = 0; i < 256; i++)
            virtual_colors[i] = (uint8_t)i;
        virtual_init = 1;
    }
    virtual_colors[index & 255] = (uint8_t)value;
}

int
fg_getindex(int index)
{
    return virtual_init ? virtual_colors[index & 255] : index;
}

void
fg_setfunc(int mode)
{
    static const uint8_t ops[] = { OP_COPY, OP_AND, OP_OR, OP_XOR };
    __g.op = ops[mode & 3];
}

/* ---- Pages and clipping. ---- */

void
fg_setpage(int page)
{
    setactivepage(page);
    fg_page = page;
}

int
fg_getpage(void)
{
    return fg_page;
}

void
fg_setvpage(int page)
{
    setvisualpage(page);
    fg_vpage = page;
}

int
fg_getvpage(void)
{
    return fg_vpage;
}

static uint16_t
page_seg(int page)
{
    uint16_t base = __g.m->bios == 0xffff ? 0xb000 : __g.seg - fg_page * __g.m->pagesize;
    return base + page * __g.m->pagesize;
}

void
fg_transfer(int minx, int maxx, int miny, int maxy, int newx, int newy, int src, int dst)
{
    if (!__g.active || src >= __g.m->pages || dst >= __g.m->pages)
        return;
    uint16_t saved = __g.seg, s = page_seg(src), d = page_seg(dst);
    uint8_t op = __g.op;
    __g.op = OP_COPY;
    /* Bottom up, so a copy within a page down the screen works. */
    for (int y = maxy; y >= miny; y--) {
        int ty = newy - (maxy - y);
        if (y < 0 || y > __g.maxy || ty < 0 || ty > __g.maxy)
            continue;
        for (int x = minx; x <= maxx; x++) {
            int tx = newx + (x - minx);
            if (x < 0 || x > __g.maxx || tx < 0 || tx > __g.maxx)
                continue;
            __g.seg = s;
            unsigned c = __g_get(x, y);
            __g.seg = d;
            __g_plot(tx, ty, (int)c);
        }
    }
    __g.seg = saved;
    __g.op = op;
}

void
fg_copypage(int src, int dst)
{
    fg_transfer(0, __g.maxx, 0, __g.maxy, 0, __g.maxy, src, dst);
}

void
fg_setclip(int minx, int maxx, int miny, int maxy)
{
    __g.vl = __g.vt = 0;
    __g.cl = minx < 0 ? 0 : minx;
    __g.ct = miny < 0 ? 0 : miny;
    __g.cr = maxx > __g.maxx ? __g.maxx : maxx;
    __g.cb = maxy > __g.maxy ? __g.maxy : maxy;
}

/* ---- Drawing. ---- */

void
fg_erase(void)
{
    uint8_t op = __g.op;
    __g_clear();
    __g.op = op;
}

void
fg_fillpage(void)
{
    uint8_t op = __g.op;
    __g.op = OP_COPY;
    for (int y = 0; y <= __g.maxy; y++)
        __g_hline(0, __g.maxx, y, __g.color);
    __g.op = op;
}

void
fg_point(int x, int y)
{
    __g_pixel(x, y, __g.color);
}

void
fg_pointx(int x, int y)
{
    uint8_t op = __g.op;
    __g.op = OP_XOR;
    __g_pixel(x, y, __g.color);
    __g.op = op;
}

int
fg_getpixel(int x, int y)
{
    if (x < 0 || x > __g.maxx || y < 0 || y > __g.maxy)
        return 0;
    return (int)__g_get(x, y);
}

void
fg_move(int x, int y)
{
    __g.cpx = x;
    __g.cpy = y;
}

void
fg_moverel(int dx, int dy)
{
    __g.cpx += dx;
    __g.cpy += dy;
}

int
fg_getxpos(void)
{
    return __g.cpx;
}

int
fg_getypos(void)
{
    return __g.cpy;
}

static void
solid_line(int x1, int y1, int x2, int y2, uint16_t pattern)
{
    uint16_t lp = __g.linepat;
    int th = __g.thickness;
    __g.linepat = pattern;
    __g.thickness = NORM_WIDTH;
    __g_line(x1, y1, x2, y2);
    __g.linepat = lp;
    __g.thickness = th;
}

void
fg_draw(int x, int y)
{
    solid_line(__g.cpx, __g.cpy, x, y, 0xffff);
    fg_move(x, y);
}

void
fg_drawrel(int dx, int dy)
{
    fg_draw(__g.cpx + dx, __g.cpy + dy);
}

void
fg_drawx(int x, int y)
{
    uint8_t op = __g.op;
    __g.op = OP_XOR;
    fg_draw(x, y);
    __g.op = op;
}

void
fg_dash(int x, int y, int pattern)
{
    solid_line(__g.cpx, __g.cpy, x, y, (uint16_t)pattern);
    fg_move(x, y);
}

void
fg_dashrel(int dx, int dy, int pattern)
{
    fg_dash(__g.cpx + dx, __g.cpy + dy, pattern);
}

void
fg_rect(int minx, int maxx, int miny, int maxy)
{
    for (int y = miny; y <= maxy; y++)
        __g_span(minx, maxx, y, __g.color);
}

void
fg_box(int minx, int maxx, int miny, int maxy)
{
    int dx = box_dx, dy = box_dy;
    fg_rect(minx, maxx, miny, miny + dy - 1);
    fg_rect(minx, maxx, maxy - dy + 1, maxy);
    fg_rect(minx, minx + dx - 1, miny + dy, maxy - dy);
    fg_rect(maxx - dx + 1, maxx, miny + dy, maxy - dy);
}

void
fg_boxx(int minx, int maxx, int miny, int maxy)
{
    uint8_t op = __g.op;
    __g.op = OP_XOR;
    fg_box(minx, maxx, miny, maxy);
    __g.op = op;
}

void
fg_boxdepth(int xdepth, int ydepth)
{
    box_dx = xdepth < 1 ? 1 : xdepth;
    box_dy = ydepth < 1 ? 1 : ydepth;
}

void
fg_circle(int r)
{
    uint8_t op = __g.op;
    int th = __g.thickness;
    __g.thickness = NORM_WIDTH;
    circle(__g.cpx, __g.cpy, r);
    __g.thickness = th;
    __g.op = op;
}

void
fg_ellipse(int rx, int ry)
{
    uint8_t op = __g.op;
    int th = __g.thickness;
    __g.thickness = NORM_WIDTH;
    ellipse(__g.cpx, __g.cpy, 0, 360, rx, ry);
    __g.thickness = th;
    __g.op = op;
}

static void
solid_fill(void)
{
    __g.fillstyle = SOLID_FILL;
    __g.fillcolor = __g.color;
}

void
fg_ellipsef(int rx, int ry)
{
    int fs = __g.fillstyle, fc = __g.fillcolor;
    uint8_t op = __g.op;
    solid_fill();
    fillellipse(__g.cpx, __g.cpy, rx, ry);
    __g.fillstyle = fs;
    __g.fillcolor = fc;
    __g.op = op;
}

void
fg_circlef(int r)
{
    fg_ellipsef(r, (int)((long)r * __g.xasp / __g.yasp));
}

void
fg_polygon(const int *xs, const int *ys, int n)
{
    for (int i = 0; i < n; i++) {
        int j = i + 1 == n ? 0 : i + 1;
        solid_line(xs[i], ys[i], xs[j], ys[j], 0xffff);
    }
}

void
fg_polyline(const int *v, int n)
{
    for (int i = 0; i + 1 < n; i++)
        solid_line(v[2 * i], v[2 * i + 1], v[2 * i + 2], v[2 * i + 3], 0xffff);
}

void
fg_polyfill(const int *v, int *work, int n)
{
    (void)work;
    int fs = __g.fillstyle, fc = __g.fillcolor, wm = __g.writemode;
    solid_fill();
    __g.writemode = COPY_PUT;
    fillpoly(n, v);
    __g.fillstyle = fs;
    __g.fillcolor = fc;
    __g.writemode = wm;
}

/* Fill the region of the seed's color, around (x, y), with the color. */
#define PAINT_STACK 512
static struct {
    int16_t x, y;
} paint_stack[PAINT_STACK];

void
fg_paint(int x, int y)
{
    if (x < __g.cl || x > __g.cr || y < __g.ct || y > __g.cb)
        return;
    unsigned seed = __g_get(x, y), c = (unsigned)__g.color;
    if (seed == c)
        return;
    uint8_t op = __g.op;
    __g.op = OP_COPY;
    int sp = 0;
    paint_stack[sp].x = (int16_t)x;
    paint_stack[sp++].y = (int16_t)y;
    while (sp) {
        sp--;
        x = paint_stack[sp].x;
        y = paint_stack[sp].y;
        if (__g_get(x, y) != seed)
            continue;
        int l = x, r = x;
        while (l > __g.cl && __g_get(l - 1, y) == seed)
            l--;
        while (r < __g.cr && __g_get(r + 1, y) == seed)
            r++;
        __g_hline(l, r, y, (int)c);
        for (int ny = y - 1; ny <= y + 1; ny += 2) {
            if (ny < __g.ct || ny > __g.cb)
                continue;
            int prev = 0;
            for (int i = l; i <= r; i++) {
                int in = __g_get(i, ny) == seed;
                if (in && !prev && sp < PAINT_STACK) {
                    paint_stack[sp].x = (int16_t)i;
                    paint_stack[sp++].y = (int16_t)ny;
                }
                prev = in;
            }
        }
    }
    __g.op = op;
}

void
fg_flood(int x, int y)
{
    fg_paint(x, y);
}

/* ---- Text. ---- */

void
fg_locate(int row, int col)
{
    text_row = row;
    text_col = col;
    if (!__g.active)
        __g_int10(0x0200, (uint16_t)*bda(0x62) << 8, 0, (uint16_t)(row << 8 | col));
}

void
fg_where(int *row, int *col)
{
    *row = text_row;
    *col = text_col;
}

/* Draw n characters with the top left of the first at (x, y). */
static void
draw_text(int x, int y, const char *s, int n)
{
    char buf[81];
    int saved_h = __g.hjust, saved_v = __g.vjust, saved_d = __g.dir, saved_s = __g.charsize;
    __g.hjust = LEFT_TEXT;
    __g.vjust = TOP_TEXT;
    __g.dir = HORIZ_DIR;
    __g.charsize = 1;
    while (n > 0) {
        int k = n > 80 ? 80 : n;
        memcpy(buf, s, (size_t)k);
        buf[k] = 0;
        outtextxy(x, y, buf);
        x += k * 8;
        s += k;
        n -= k;
    }
    __g.hjust = saved_h;
    __g.vjust = saved_v;
    __g.dir = saved_d;
    __g.charsize = saved_s;
}

void
fg_text(const char *s, int n)
{
    if (!__g.active) {
        /* Text modes: through the BIOS, in the current color. */
        uint16_t page = (uint16_t)*bda(0x62) << 8;
        for (int i = 0; i < n; i++) {
            __g_int10(0x0200, page, 0, (uint16_t)(text_row << 8 | text_col));
            __g_int10(0x0900 | (uint8_t)s[i], page | (uint8_t)(__g.color ? __g.color : 7), 1, 0);
            text_col++;
        }
        __g_int10(0x0200, page, 0, (uint16_t)(text_row << 8 | text_col));
        return;
    }
    draw_text(text_col * 8, text_row * 8, s, n);
    text_col += n;
}

void
fg_textc(const char *s, int n)
{
    fg_text(s, n);
}

void
fg_justify(int xjust, int yjust)
{
    just_x = xjust;
    just_y = yjust;
}

void
fg_print(const char *s, int n)
{
    int w = n * 8, x = __g.cpx, y = __g.cpy;
    if (just_x == 0)
        x -= w / 2;
    else if (just_x > 0)
        x -= w - 1;
    if (just_y < 0)
        y -= 7;
    else if (just_y == 0)
        y -= 4;
    draw_text(x, y, s, n);
}

int
fg_fontsize(int size)
{
    return size == 8 ? 8 : 0;
}

/* ---- Images. ---- */

int
fg_imagesiz(int w, int h)
{
    return w * h;
}

/* Rows of an image go from the top; the graphics cursor is the lower
 * left corner. */
void
fg_getimage(char *map, int w, int h)
{
    int x0 = __g.cpx, y0 = __g.cpy - h + 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            map[y * w + x] = (char)fg_getpixel(x0 + x, y0 + y);
}

static void
put(const char *map, int w, int h, int transparent, int mirror)
{
    int x0 = __g.cpx, y0 = __g.cpy - h + 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t c = (uint8_t)map[y * w + (mirror ? w - 1 - x : x)];
            if (!transparent || c)
                __g_pixel(x0 + x, y0 + y, c);
        }
}

void
fg_putimage(const char *map, int w, int h)
{
    put(map, w, h, 0, 0);
}

void
fg_drwimage(const char *map, int w, int h)
{
    put(map, w, h, 1, 0);
}

void
fg_clpimage(const char *map, int w, int h)
{
    put(map, w, h, 1, 0);
}

void
fg_revimage(const char *map, int w, int h)
{
    put(map, w, h, 1, 1);
}

void
fg_flpimage(const char *map, int w, int h)
{
    put(map, w, h, 1, 1);
}

/* Bitmaps: width in bytes, a bit a pixel, the high bit at the left. */
void
fg_getmap(char *map, int w, int h)
{
    int x0 = __g.cpx, y0 = __g.cpy - h + 1;
    for (int y = 0; y < h; y++)
        for (int b = 0; b < w; b++) {
            uint8_t v = 0;
            for (int i = 0; i < 8; i++)
                if (fg_getpixel(x0 + b * 8 + i, y0 + y))
                    v |= (uint8_t)(0x80 >> i);
            map[y * w + b] = (char)v;
        }
}

void
fg_drawmap(const char *map, int w, int h)
{
    int x0 = __g.cpx, y0 = __g.cpy - h + 1;
    for (int y = 0; y < h; y++)
        for (int b = 0; b < w; b++) {
            uint8_t v = (uint8_t)map[y * w + b];
            for (int i = 0; i < 8; i++)
                if (v & (0x80 >> i))
                    __g_pixel(x0 + b * 8 + i, y0 + y, __g.color);
        }
}

/* ---- Keyboard. ---- */

static uint16_t
int16(uint16_t ax)
{
    __asm__ __volatile__("int $0x16" : "+a"(ax) : : "cc");
    return ax;
}

static int
key_ready(uint16_t *peek)
{
    uint16_t ax = 0x0100, flags;
    __asm__ __volatile__("int $0x16\n\tpushfw\n\tpopw %1" : "+a"(ax), "=r"(flags) : : "cc");
    if (peek)
        *peek = ax;
    return !(flags & 0x40);
}

static void
split(uint16_t k, unsigned char *key, unsigned char *aux)
{
    if ((k & 0xff) == 0 || (k & 0xff) == 0xe0) {
        *key = 0;
        *aux = (unsigned char)(k >> 8);
    } else {
        *key = (unsigned char)k;
        *aux = 0;
    }
}

void
fg_getkey(unsigned char *key, unsigned char *aux)
{
    split(int16(0x0000), key, aux);
}

void
fg_intkey(unsigned char *key, unsigned char *aux)
{
    if (key_ready(0))
        split(int16(0x0000), key, aux);
    else
        *key = *aux = 0;
}

void
fg_waitkey(void)
{
    while (key_ready(0))
        int16(0x0000);
    int16(0x0000);
}

int
fg_kbtest(int scan)
{
    uint16_t k;
    if (!key_ready(&k))
        return 0;
    return scan == 0 || (k >> 8) == scan;
}

/* ---- Mouse. ---- */

static int mouse_xs = 1; /* the driver counts 640 across in 320-wide modes */

static uint16_t
int33(uint16_t ax, uint16_t *bx, uint16_t *cx, uint16_t *dx)
{
    uint16_t b = bx ? *bx : 0, c = cx ? *cx : 0, d = dx ? *dx : 0;
    __asm__ __volatile__("pushw %%bp\n\tint $0x33\n\tpopw %%bp"
                         : "+a"(ax), "+b"(b), "+c"(c), "+d"(d)
                         :
                         : "si", "di", "memory", "cc");
    if (bx)
        *bx = b;
    if (cx)
        *cx = c;
    if (dx)
        *dx = d;
    return ax;
}

int
fg_mouseini(void)
{
    const uint16_t __far *vec = (const uint16_t __far *)GFAR(0, 0x33 * 4);
    if (!vec[0] && !vec[1])
        return 0;
    uint16_t bx = 0;
    if (int33(0, &bx, 0, 0) != 0xffff)
        return 0;
    mouse_xs = __g.active && __g.m->width == 320 ? 2 : 1;
    if (__g.active)
        fg_mouselim(0, __g.maxx, 0, __g.maxy);
    return bx == 0xffff ? 2 : bx;
}

void
fg_mousevis(int state)
{
    int33(state ? 1 : 2, 0, 0, 0);
}

void
fg_mousepos(int *x, int *y, int *buttons)
{
    uint16_t bx = 0, cx = 0, dx = 0;
    int33(3, &bx, &cx, &dx);
    *x = cx / mouse_xs;
    *y = dx;
    *buttons = bx & 7;
}

void
fg_mousemov(int x, int y)
{
    uint16_t cx = (uint16_t)(x * mouse_xs), dx = (uint16_t)y;
    int33(4, 0, &cx, &dx);
}

void
fg_mouselim(int minx, int maxx, int miny, int maxy)
{
    uint16_t cx = (uint16_t)(minx * mouse_xs), dx = (uint16_t)(maxx * mouse_xs);
    int33(7, 0, &cx, &dx);
    cx = (uint16_t)miny;
    dx = (uint16_t)maxy;
    int33(8, 0, &cx, &dx);
}

void
fg_mousebut(int number, int *count, int *lastx, int *lasty)
{
    /* number > 0: presses of button number; < 0: releases. */
    uint16_t bx = (uint16_t)((number < 0 ? -number : number) - 1), cx = 0, dx = 0;
    int33(number < 0 ? 6 : 5, &bx, &cx, &dx);
    *count = bx;
    *lastx = cx / mouse_xs;
    *lasty = dx;
}

/* ---- Time and sound. ---- */

void
fg_waitfor(int n)
{
    uint32_t start = ticks();
    while ((int32_t)(ticks() - start) < n)
        ;
}

long
fg_getclock(void)
{
    return (long)ticks();
}

void
fg_stall(int n)
{
    /* About n hundredths of a tick; Fastgraph calibrates with fg_measure. */
    for (volatile int i = 0; i < n; i++)
        for (volatile int j = 0; j < 20; j++)
            ;
}

int
fg_measure(void)
{
    return 1;
}

void
fg_sound(int freq, int duration)
{
    if (freq < 19)
        freq = 19;
    uint16_t div = (uint16_t)(1193182UL / (unsigned)freq);
    __g_outb(0x43, 0xb6);
    __g_outb(0x42, (uint8_t)div);
    __g_outb(0x42, (uint8_t)(div >> 8));
    __g_outb(0x61, __g_inb(0x61) | 3);
    if (duration > 0) {
        fg_waitfor(duration);
        fg_quiet();
    }
}

void
fg_quiet(void)
{
    __g_outb(0x61, __g_inb(0x61) & ~3);
}

void
fg_beep(void)
{
    fg_sound(800, 3);
}
