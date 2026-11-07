/* Drawing: pixels, lines, outlines, arcs and filled shapes.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "graph-local.h"
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

static const int16_t sintab[91] = {
    0, 286, 572, 857, 1143, 1428, 1713, 1997, 2280, 2563, 2845, 3126, 3406,
    3686, 3964, 4240, 4516, 4790, 5063, 5334, 5604, 5872, 6138, 6402, 6664,
    6924, 7182, 7438, 7692, 7943, 8192, 8438, 8682, 8923, 9162, 9397, 9630,
    9860, 10087, 10311, 10531, 10749, 10963, 11174, 11381, 11585, 11786,
    11982, 12176, 12365, 12551, 12733, 12911, 13085, 13255, 13421, 13583,
    13741, 13894, 14044, 14189, 14330, 14466, 14598, 14726, 14849, 14968,
    15082, 15191, 15296, 15396, 15491, 15582, 15668, 15749, 15826, 15897,
    15964, 16026, 16083, 16135, 16182, 16225, 16262, 16294, 16322, 16344,
    16362, 16374, 16382, 16384,
};

int
__g_sin(int d)
{
    d %= 360;
    if (d < 0)
        d += 360;
    if (d <= 90)
        return sintab[d];
    if (d <= 180)
        return sintab[180 - d];
    if (d <= 270)
        return -sintab[d - 180];
    return -sintab[360 - d];
}

int
__g_cos(int d)
{
    return __g_sin(d % 360 + 90);
}

/* The color of the background, as fills and clears use it. */
static int
bgc(void)
{
    return __g.m->layout == LAYOUT_LINEAR ? __g.bkcolor : 0;
}

/* ---- Pixels and spans. ---- */

void
__g_pixel(int x, int y, int c)
{
    x += __g.vl;
    y += __g.vt;
    if (x < __g.cl || x > __g.cr || y < __g.ct || y > __g.cb)
        return;
    __g_plot(x, y, c);
}

/* Clip the span x1..x2 of row y, in absolute coordinates. */
static int
clip_span(int *x1, int *x2, int y)
{
    int l = __g.cl, r = __g.cr, t = __g.ct, b = __g.cb;
    if (*x1 > *x2) {
        int tmp = *x1;
        *x1 = *x2;
        *x2 = tmp;
    }
    if (y < t || y > b || *x2 < l || *x1 > r)
        return 0;
    if (*x1 < l)
        *x1 = l;
    if (*x2 > r)
        *x2 = r;
    return 1;
}

void
__g_span(int x1, int x2, int y, int c)
{
    x1 += __g.vl;
    x2 += __g.vl;
    y += __g.vt;
    if (clip_span(&x1, &x2, y))
        __g_hline(x1, x2, y, c);
}

void
__g_fillspan(int x1, int x2, int y)
{
    x1 += __g.vl;
    x2 += __g.vl;
    y += __g.vt;
    if (!clip_span(&x1, &x2, y))
        return;
    uint8_t op = __g.op;
    __g.op = OP_COPY;
    if (__g.fillstyle == SOLID_FILL) {
        __g_hline(x1, x2, y, __g.fillcolor);
    } else if (__g.fillstyle == EMPTY_FILL) {
        __g_hline(x1, x2, y, bgc());
    } else {
        uint8_t row = __g.fillpat[y & 7];
        int fg = __g.fillcolor, bg = bgc();
        while (x1 <= x2) {
            int bit = (row >> (7 - (x1 & 7))) & 1, x = x1;
            while (x < x2 && ((row >> (7 - ((x + 1) & 7))) & 1) == bit)
                x++;
            __g_hline(x1, x, y, bit ? fg : bg);
            x1 = x + 1;
        }
    }
    __g.op = op;
}

/* ---- Lines. ---- */

static void
thick_pixel(int x, int y, int c, int xmajor)
{
    if (__g.thickness != THICK_WIDTH) {
        __g_pixel(x, y, c);
    } else if (xmajor) {
        __g_pixel(x, y - 1, c);
        __g_pixel(x, y, c);
        __g_pixel(x, y + 1, c);
    } else {
        __g_pixel(x - 1, y, c);
        __g_pixel(x, y, c);
        __g_pixel(x + 1, y, c);
    }
}

void
__g_line(int x1, int y1, int x2, int y2)
{
    int c = __g.color;
    uint16_t pat = __g.linepat;
    if (y1 == y2 && pat == 0xffff && __g.thickness != THICK_WIDTH) {
        int a = x1 + __g.vl, b = x2 + __g.vl;
        if (clip_span(&a, &b, y1 + __g.vt))
            __g_hline(a, b, y1 + __g.vt, c);
        return;
    }
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy, xmajor = dx >= dy;
    unsigned i = 0;
    for (;;) {
        if (pat & (0x8000u >> (i & 15)))
            thick_pixel(x1, y1, c, xmajor);
        i++;
        if (x1 == x2 && y1 == y2)
            break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void
putpixel(int x, int y, int color)
{
    __g.op = OP_COPY;
    __g_pixel(x, y, color);
}

unsigned
getpixel(int x, int y)
{
    x += __g.vl;
    y += __g.vt;
    if (x < 0 || x > __g.maxx || y < 0 || y > __g.maxy)
        return 0;
    return __g_get(x, y);
}

void
moveto(int x, int y)
{
    __g.cpx = x;
    __g.cpy = y;
}

void
moverel(int dx, int dy)
{
    __g.cpx += dx;
    __g.cpy += dy;
}

int
getx(void)
{
    return __g.cpx;
}

int
gety(void)
{
    return __g.cpy;
}

static const uint16_t line_patterns[] = { 0xffff, 0xcccc, 0xff18, 0xf8f8 };

void
setlinestyle(int style, unsigned pattern, int thickness)
{
    if (style < SOLID_LINE || style > USERBIT_LINE ||
        (thickness != NORM_WIDTH && thickness != THICK_WIDTH)) {
        __g.result = grError;
        return;
    }
    __g.linestyle = style;
    __g.linepat = style == USERBIT_LINE ? (uint16_t)pattern : line_patterns[style];
    __g.thickness = thickness;
}

void
getlinesettings(struct linesettingstype *li)
{
    li->linestyle = __g.linestyle;
    li->upattern = __g.linepat;
    li->thickness = __g.thickness;
}

void
setwritemode(int mode)
{
    if (mode == COPY_PUT || mode == XOR_PUT)
        __g.writemode = mode;
}

static void
begin_lines(void)
{
    __g.op = __g.writemode == XOR_PUT ? OP_XOR : OP_COPY;
}

void
line(int x1, int y1, int x2, int y2)
{
    begin_lines();
    __g_line(x1, y1, x2, y2);
}

void
lineto(int x, int y)
{
    begin_lines();
    __g_line(__g.cpx, __g.cpy, x, y);
    __g.cpx = x;
    __g.cpy = y;
}

void
linerel(int dx, int dy)
{
    lineto(__g.cpx + dx, __g.cpy + dy);
}

void
rectangle(int left, int top, int right, int bottom)
{
    begin_lines();
    __g_line(left, top, right, top);
    __g_line(left, bottom, right, bottom);
    if (bottom - top > 1 || top - bottom > 1) {
        int t = top < bottom ? top + 1 : bottom + 1, b = top < bottom ? bottom - 1 : top - 1;
        __g_line(left, t, left, b);
        __g_line(right, t, right, b);
    }
}

void
drawpoly(int n, const int *p)
{
    begin_lines();
    for (int i = 0; i + 1 < n; i++)
        __g_line(p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3]);
}

/* ---- Fill styles. ---- */

static const uint8_t fill_patterns[USER_FILL][8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* EMPTY_FILL */
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, /* SOLID_FILL */
    { 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00 }, /* LINE_FILL */
    { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 }, /* LTSLASH_FILL */
    { 0x07, 0x0e, 0x1c, 0x38, 0x70, 0xe0, 0xc1, 0x83 }, /* SLASH_FILL */
    { 0xe0, 0x70, 0x38, 0x1c, 0x0e, 0x07, 0x83, 0xc1 }, /* BKSLASH_FILL */
    { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 }, /* LTBKSLASH_FILL */
    { 0xff, 0x88, 0x88, 0x88, 0xff, 0x88, 0x88, 0x88 }, /* HATCH_FILL */
    { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 }, /* XHATCH_FILL */
    { 0xcc, 0x33, 0xcc, 0x33, 0xcc, 0x33, 0xcc, 0x33 }, /* INTERLEAVE_FILL */
    { 0x80, 0x00, 0x08, 0x00, 0x80, 0x00, 0x08, 0x00 }, /* WIDE_DOT_FILL */
    { 0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00 }, /* CLOSE_DOT_FILL */
};

void
setfillstyle(int pattern, int color)
{
    if (pattern < EMPTY_FILL || pattern > USER_FILL) {
        __g.result = grError;
        return;
    }
    __g.fillstyle = pattern;
    __g.fillcolor = color & __g.maxcolor;
    for (int i = 0; i < 8; i++)
        __g.fillpat[i] = pattern == USER_FILL ? __g.userpat[i] : fill_patterns[pattern][i];
}

void
setfillpattern(const char *pattern, int color)
{
    for (int i = 0; i < 8; i++)
        __g.userpat[i] = (uint8_t)pattern[i];
    setfillstyle(USER_FILL, color);
}

void
getfillsettings(struct fillsettingstype *fi)
{
    fi->pattern = __g.fillstyle;
    fi->color = __g.fillcolor;
}

void
getfillpattern(char *pattern)
{
    for (int i = 0; i < 8; i++)
        pattern[i] = (char)__g.userpat[i];
}

/* ---- Filled shapes. ---- */

void
bar(int left, int top, int right, int bottom)
{
    if (top > bottom) {
        int t = top;
        top = bottom;
        bottom = t;
    }
    for (int y = top; y <= bottom; y++)
        __g_fillspan(left, right, y);
}

void
bar3d(int left, int top, int right, int bottom, int depth, int topflag)
{
    bar(left, top, right, bottom);
    rectangle(left, top, right, bottom);
    if (depth) {
        int dy = depth * 3 / 4;
        __g_line(right, bottom, right + depth, bottom - dy);
        __g_line(right + depth, bottom - dy, right + depth, top - dy);
        if (topflag) {
            __g_line(right, top, right + depth, top - dy);
            __g_line(left, top, left + depth, top - dy);
            __g_line(left + depth, top - dy, right + depth, top - dy);
        }
    }
}

/* Fill the inside of a polygon, by the even-odd rule. */
#define MAXCROSS 64
static void
poly_fill(int n, const int *p)
{
    if (n < 3)
        return;
    int ymin = p[1], ymax = p[1];
    for (int i = 1; i < n; i++) {
        if (p[2 * i + 1] < ymin)
            ymin = p[2 * i + 1];
        if (p[2 * i + 1] > ymax)
            ymax = p[2 * i + 1];
    }
    /* Only the rows that can be seen. */
    int top = __g.ct - __g.vt, bottom = __g.cb - __g.vt;
    if (ymin < top)
        ymin = top;
    if (ymax > bottom)
        ymax = bottom;
    static int xs[MAXCROSS];
    for (int y = ymin; y <= ymax; y++) {
        int k = 0;
        for (int i = 0; i < n; i++) {
            int j = i + 1 == n ? 0 : i + 1;
            int x0 = p[2 * i], y0 = p[2 * i + 1], x1 = p[2 * j], y1 = p[2 * j + 1];
            if (y0 == y1)
                continue;
            if (y0 > y1) {
                int t = x0;
                x0 = x1;
                x1 = t;
                t = y0;
                y0 = y1;
                y1 = t;
            }
            if (y < y0 || y >= y1)
                continue;
            int x = x0 + (int)((long)(y - y0) * (x1 - x0) / (y1 - y0));
            if (k == MAXCROSS) {
                __g.result = grNoScanMem;
                break;
            }
            int m = k++;
            while (m > 0 && xs[m - 1] > x) {
                xs[m] = xs[m - 1];
                m--;
            }
            xs[m] = x;
        }
        for (int i = 0; i + 1 < k; i += 2)
            __g_fillspan(xs[i], xs[i + 1], y);
    }
}

void
fillpoly(int n, const int *p)
{
    poly_fill(n, p);
    drawpoly(n, p);
    if (n > 1) {
        begin_lines();
        __g_line(p[2 * n - 2], p[2 * n - 1], p[0], p[1]);
    }
}

/* ---- Ellipses and arcs. ---- */

/* A number that grows with the angle of (x, y), y up, from 0 at 0 degrees
 * to 4096 at 360, for comparing angles without trigonometry. */
static long
pseudo_angle(long x, long y)
{
    const long K = 1024;
    if (x == 0 && y == 0)
        return 0;
    if (y >= 0)
        return x > 0 ? y * K / (x + y) : K + (-x) * K / (-x + y);
    return x < 0 ? 2 * K + (-y) * K / (-x - y) : 3 * K + x * K / (x - y);
}

static struct {
    int cx, cy, rx, ry;
    int full;
    long ps, pe;
    int wrap;
    uint8_t thick;
} el;

static void
el_point(int dx, int dy)
{
    if (!el.full) {
        long a = pseudo_angle((long)dx * el.ry, -(long)dy * el.rx);
        if (el.wrap ? (a < el.ps && a > el.pe) : (a < el.ps || a > el.pe))
            return;
    }
    int x = el.cx + dx, y = el.cy + dy, c = __g.color;
    if (el.thick) {
        for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++)
                __g_pixel(x + i, y + j, c);
    } else {
        __g_pixel(x, y, c);
    }
}

static void
el_quad(int x, int y)
{
    el_point(x, y);
    if (x)
        el_point(-x, y);
    if (y) {
        el_point(x, -y);
        if (x)
            el_point(-x, -y);
    }
}

/* The outline of the ellipse, by the midpoint method. */
static void
el_outline(void)
{
    int rx = el.rx, ry = el.ry;
    if (rx <= 0 || ry <= 0) {
        if (rx <= 0 && ry <= 0) {
            el_point(0, 0);
        } else if (rx <= 0) {
            for (int y = -ry; y <= ry; y++)
                el_point(0, y);
        } else {
            for (int x = -rx; x <= rx; x++)
                el_point(x, 0);
        }
        return;
    }
    long rx2 = (long)rx * rx, ry2 = (long)ry * ry;
    int x = 0, y = ry;
    long px = 0, py = 2 * rx2 * y;
    long p = ry2 - rx2 * ry + rx2 / 4;
    el_quad(x, y);
    while (px < py) {
        x++;
        px += 2 * ry2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= 2 * rx2;
            p += ry2 + px - py;
        }
        el_quad(x, y);
    }
    /* Region 2 starts from F(x + 1/2, y - 1), worked out in 64 bits as
     * its terms are too large for 32. */
    long long p2 = (long long)ry2 * (2 * x + 1) * (2 * x + 1) / 4 +
                   (long long)rx2 * (y - 1) * (y - 1) - (long long)rx2 * ry2;
    p = (long)p2;
    while (y > 0) {
        y--;
        py -= 2 * rx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += 2 * ry2;
            p += rx2 - py + px;
        }
        el_quad(x, y);
    }
}

static void
set_arc(int x, int y, int st, int end, int rx, int ry)
{
    el.cx = x;
    el.cy = y;
    el.rx = rx;
    el.ry = ry;
    el.thick = __g.thickness == THICK_WIDTH;
    el.full = end - st >= 360 || st - end >= 360;
    st %= 360;
    if (st < 0)
        st += 360;
    end %= 360;
    if (end < 0)
        end += 360;
    el.ps = pseudo_angle(__g_cos(st), __g_sin(st));
    el.pe = pseudo_angle(__g_cos(end), __g_sin(end));
    if (end == 0 && st != 0)
        el.pe = 4096; /* up to 360 */
    el.wrap = el.pe < el.ps;
    __g.arc.x = x;
    __g.arc.y = y;
    __g.arc.xstart = x + (int)((long)rx * __g_cos(st) / 16384);
    __g.arc.ystart = y - (int)((long)ry * __g_sin(st) / 16384);
    __g.arc.xend = x + (int)((long)rx * __g_cos(end) / 16384);
    __g.arc.yend = y - (int)((long)ry * __g_sin(end) / 16384);
}

static int
aspect_y(int r)
{
    return (int)((long)r * __g.xasp / __g.yasp);
}

void
ellipse(int x, int y, int st, int end, int rx, int ry)
{
    __g.op = OP_COPY;
    set_arc(x, y, st, end, rx, ry);
    el_outline();
}

void
arc(int x, int y, int st, int end, int r)
{
    ellipse(x, y, st, end, r, aspect_y(r));
}

void
circle(int x, int y, int r)
{
    ellipse(x, y, 0, 360, r, aspect_y(r));
}

void
getarccoords(struct arccoordstype *ac)
{
    *ac = __g.arc;
}

static long
isqrt(long v)
{
    long r = 0, bit = 1L << 30;
    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

void
fillellipse(int x, int y, int rx, int ry)
{
    __g.op = OP_COPY;
    long ry2 = (long)ry * ry;
    for (int dy = -ry; dy <= ry; dy++) {
        int dx = ry ? (int)((long)rx * isqrt(ry2 - (long)dy * dy) / ry) : rx;
        __g_fillspan(x - dx, x + dx, y + dy);
    }
    set_arc(x, y, 0, 360, rx, ry);
    el_outline();
}

void
sector(int x, int y, int st, int end, int rx, int ry)
{
    __g.op = OP_COPY;
    /* The inside, as a polygon through the center and points of the arc. */
    while (end < st)
        end += 360;
    if (end - st > 360)
        end = st + 360;
    static int pts[2 * 125];
    int n = 0;
    pts[n++] = x;
    pts[n++] = y;
    for (int a = st;; a += 3) {
        if (a > end)
            a = end;
        pts[n++] = x + (int)((long)rx * __g_cos(a) / 16384);
        pts[n++] = y - (int)((long)ry * __g_sin(a) / 16384);
        if (a == end)
            break;
    }
    poly_fill(n / 2, pts);
    set_arc(x, y, st, end, rx, ry);
    el_outline();
    if (end - st < 360) {
        __g_line(x, y, __g.arc.xstart, __g.arc.ystart);
        __g_line(x, y, __g.arc.xend, __g.arc.yend);
    }
}

void
pieslice(int x, int y, int st, int end, int r)
{
    sector(x, y, st, end, r, aspect_y(r));
}

/* ---- Flood fill. ---- */

#define FLOOD_STACK 512
static struct {
    int16_t x, y;
} flood_stack[FLOOD_STACK];

void
floodfill(int x, int y, int border)
{
    __g.op = OP_COPY;
    int wl = __g.cl, wr = __g.cr, wt = __g.ct, wb = __g.cb;
    x += __g.vl;
    y += __g.vt;
    if (x < wl || x > wr || y < wt || y > wb)
        return;
    /* A bit for each pixel already filled: the fill pattern can leave any
     * color behind, so the screen cannot tell. Without the memory, a
     * solid fill stops at pixels of the fill color too. */
    unsigned rowbytes = (unsigned)(wr - wl + 8) >> 3;
    uint32_t size = (uint32_t)rowbytes * (unsigned)(wb - wt + 1);
    uint8_t __far *seen = size < 0xfff0u ? _fmalloc((size_t)size) : 0;
    if (seen)
        _fmemset(seen, 0, (size_t)size);
    unsigned fc = __g.fillstyle == EMPTY_FILL ? (unsigned)bgc() : (unsigned)__g.fillcolor;
#define SEEN(px, py) (seen[(unsigned)((py) - wt) * rowbytes + ((unsigned)((px) - wl) >> 3)] & (0x80 >> (((px) - wl) & 7)))
#define INSIDE(px, py) (__g_get(px, py) != (unsigned)border && (seen ? !SEEN(px, py) : __g_get(px, py) != fc))
    int sp = 0;
    flood_stack[sp].x = (int16_t)x;
    flood_stack[sp++].y = (int16_t)y;
    while (sp > 0) {
        sp--;
        x = flood_stack[sp].x;
        y = flood_stack[sp].y;
        if (!INSIDE(x, y))
            continue;
        int xl = x, xr = x;
        while (xl > wl && INSIDE(xl - 1, y))
            xl--;
        while (xr < wr && INSIDE(xr + 1, y))
            xr++;
        __g_fillspan(xl - __g.vl, xr - __g.vl, y - __g.vt);
        if (seen)
            for (int i = xl; i <= xr; i++)
                seen[(unsigned)(y - wt) * rowbytes + ((unsigned)(i - wl) >> 3)] |= (uint8_t)(0x80 >> ((i - wl) & 7));
        for (int ny = y - 1; ny <= y + 1; ny += 2) {
            if (ny < wt || ny > wb)
                continue;
            int prev = 0;
            for (int i = xl; i <= xr; i++) {
                int in = INSIDE(i, ny);
                if (in && !prev) {
                    if (sp == FLOOD_STACK) {
                        __g.result = grNoFloodMem;
                        goto done;
                    }
                    flood_stack[sp].x = (int16_t)i;
                    flood_stack[sp++].y = (int16_t)ny;
                }
                prev = in;
            }
        }
    }
done:
    if (seen)
        _ffree(seen);
#undef SEEN
#undef INSIDE
}
