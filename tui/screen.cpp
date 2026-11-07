/* The screen buffer, drawing, the keyboard and the mouse.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "tui-local.h"
#include <stdlib.h>
#include <string.h>

namespace tui {
namespace scr {
int cols = 80, rows = 25;
uint16_t *buf;
int curX = -1, curY;
bool mouse;
int mx, my;

static uint16_t vseg;
static uint16_t *saved;       /* the screen before init */
static uint16_t savedCursor;  /* its cursor position */
static uint16_t savedShape;   /* and shape */
static unsigned char mbuttons;
static uint16_t lastClickTick;
static int lastClickX = -1, lastClickY;
static bool cursorShown = true;

typedef volatile uint8_t __far *bptr;
typedef volatile uint16_t __far *wptr;

static uint8_t
bda8(unsigned off)
{
    return *(bptr)(((uint32_t)0x40 << 16) | off);
}

static uint16_t
bda16(unsigned off)
{
    return *(wptr)(((uint32_t)0x40 << 16) | off);
}

static uint16_t
int10(uint16_t ax, uint16_t bx = 0, uint16_t cx = 0, uint16_t dx = 0)
{
    __asm__ __volatile__("pushw %%bp\n\tint $0x10\n\tpopw %%bp"
                         : "+a"(ax), "+b"(bx), "+c"(cx), "+d"(dx)
                         :
                         : "si", "di", "memory", "cc");
    return ax;
}

struct MouseRegs {
    uint16_t ax, bx, cx, dx;
};

static void
int33(MouseRegs &r)
{
    uint16_t ax = r.ax, bx = r.bx, cx = r.cx, dx = r.dx;
    __asm__ __volatile__("pushw %%bp\n\tint $0x33\n\tpopw %%bp"
                         : "+a"(ax), "+b"(bx), "+c"(cx), "+d"(dx)
                         :
                         : "si", "di", "memory", "cc");
    r.ax = ax;
    r.bx = bx;
    r.cx = cx;
    r.dx = dx;
}

static wptr
video(unsigned i)
{
    return (wptr)(((uint32_t)vseg << 16) | (uint16_t)(i * 2));
}

void
init()
{
    uint8_t mode = bda8(0x49) & 0x7f;
    if (mode != 2 && mode != 3 && mode != 7) {
        int10(0x0003);
        mode = 3;
    }
    cols = bda16(0x4a);
    uint8_t last = bda8(0x84);
    rows = (last >= 24 && last < 100) ? last + 1 : 25;
    vseg = (mode == 7 ? 0xb000 : 0xb800) + (bda16(0x4e) >> 4);
    unsigned n = (unsigned)cols * rows;
    buf = (uint16_t *)malloc(n * 2);
    saved = (uint16_t *)malloc(n * 2);
    if (saved)
        for (unsigned i = 0; i < n; i++)
            saved[i] = *video(i);
    savedCursor = bda16(0x50 + 2 * bda8(0x62));
    savedShape = bda16(0x60);
    /* Bright backgrounds instead of blinking. */
    int10(0x1003, 0);
    /* The mouse, if INT 33h points somewhere. */
    wptr vec = (wptr)(uint32_t)(0x33 * 4);
    if (vec[0] || vec[1]) {
        MouseRegs r = { 0, 0, 0, 0 };
        int33(r);
        if (r.ax == 0xffff) {
            mouse = true;
            r = { 7, 0, 0, (uint16_t)((cols - 1) * 8) };
            int33(r);
            r = { 8, 0, 0, (uint16_t)((rows - 1) * 8) };
            int33(r);
            r = { 4, 0, (uint16_t)(cols / 2 * 8), (uint16_t)(rows / 2 * 8) };
            int33(r);
            mx = cols / 2;
            my = rows / 2;
        }
    }
}

void
done()
{
    unsigned n = (unsigned)cols * rows;
    if (saved) {
        for (unsigned i = 0; i < n; i++)
            *video(i) = saved[i];
        free(saved);
        saved = 0;
    }
    int10(0x1003, 1);
    int10(0x0100, 0, savedShape);
    int10(0x0200, (uint16_t)bda8(0x62) << 8, 0, savedCursor);
    free(buf);
    buf = 0;
}

void
flush()
{
    unsigned n = (unsigned)cols * rows;
    unsigned m = mouse && mx >= 0 && mx < cols && my >= 0 && my < rows ? (unsigned)(my * cols + mx) : n;
    for (unsigned i = 0; i < n; i++) {
        uint16_t c = buf[i];
        if (i == m)
            c ^= 0x7f00;
        *video(i) = c;
    }
    if (curX >= 0 && curX < cols && curY >= 0 && curY < rows) {
        int10(0x0200, (uint16_t)bda8(0x62) << 8, 0, (uint16_t)(curY << 8 | curX));
        if (!cursorShown) {
            int10(0x0100, 0, savedShape ? savedShape : 0x0607);
            cursorShown = true;
        }
    } else if (cursorShown) {
        int10(0x0100, 0, 0x2000);
        cursorShown = false;
    }
}

static bool
keyReady()
{
    uint16_t ax = 0x0100, flags;
    __asm__ __volatile__("int $0x16\n\tpushfw\n\tpopw %1" : "+a"(ax), "=r"(flags) : : "cc");
    return !(flags & 0x40);
}

static uint16_t
int16(uint16_t ax)
{
    __asm__ __volatile__("int $0x16" : "+a"(ax) : : "cc");
    return ax;
}

bool
poll(Event &e)
{
    e = Event();
    if (keyReady()) {
        e.what = evKeyDown;
        e.key = int16(0x0000);
        /* Keys with a character but an extended scan code (the keypad's
         * Enter and /) are treated as their main keys. */
        if ((e.key & 0xff) == 0xe0)
            e.key &= 0xff00;
        e.shift = (unsigned char)int16(0x0200);
        return true;
    }
    if (!mouse)
        return false;
    MouseRegs r = { 3, 0, 0, 0 };
    int33(r);
    int x = r.cx / 8, y = r.dx / 8;
    unsigned char b = r.bx & 3;
    e.x = x;
    e.y = y;
    e.buttons = b;
    if (b && !mbuttons) {
        e.what = evMouseDown;
        uint16_t now = bda16(0x6c);
        e.dbl = x == lastClickX && y == lastClickY && (uint16_t)(now - lastClickTick) <= 8;
        lastClickTick = now;
        lastClickX = e.dbl ? -1 : x;
        lastClickY = y;
    } else if (!b && mbuttons) {
        e.what = evMouseUp;
        e.buttons = mbuttons;
    } else if (x != mx || y != my) {
        e.what = evMouseMove;
    }
    mbuttons = b;
    mx = x;
    my = y;
    return e.what != evNothing;
}

void
release()
{
    /* The DOS idle interrupt, then the multiplex call that gives the time
     * slice away under Windows and in emulators. */
    __asm__ __volatile__("int $0x28" : : : "memory");
    uint16_t ax = 0x1680;
    __asm__ __volatile__("int $0x2f" : "+a"(ax) : : "memory", "cc");
}
} // namespace scr

/* ---- Strings. ---- */

char *
dupstr(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d)
        memcpy(d, s, n);
    return d;
}

int
cstrlen(const char *s)
{
    int n = 0;
    for (; *s; s++)
        if (*s != '~')
            n++;
    return n;
}

char
hotChar(const char *s)
{
    const char *p = strchr(s, '~');
    if (!p || !p[1])
        return 0;
    char c = p[1];
    return c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
}

unsigned
altKey(char c)
{
    static const unsigned char scan[26] = {
        0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
        0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c,
    };
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    if (c >= 'A' && c <= 'Z')
        return (unsigned)scan[c - 'A'] << 8;
    if (c >= '1' && c <= '9')
        return (unsigned)(0x78 + c - '1') << 8;
    if (c == '0')
        return 0x8100;
    return 0;
}

/* ---- Drawing. ---- */

void
DrawContext::put(int x, int y, unsigned char ch, unsigned char attr) const
{
    x += ox;
    y += oy;
    if (!clip.contains(x, y) || x < 0 || y < 0 || x >= scr::cols || y >= scr::rows)
        return;
    scr::buf[y * scr::cols + x] = (uint16_t)(attr << 8 | ch);
}

void
DrawContext::fill(int x, int y, int w, int h, unsigned char ch, unsigned char attr) const
{
    int x0 = imax(x + ox, imax(clip.x, 0)), x1 = imin(x + ox + w, imin(clip.x + clip.w, scr::cols));
    int y0 = imax(y + oy, imax(clip.y, 0)), y1 = imin(y + oy + h, imin(clip.y + clip.h, scr::rows));
    uint16_t cell = (uint16_t)(attr << 8 | ch);
    for (int j = y0; j < y1; j++) {
        uint16_t *p = scr::buf + j * scr::cols;
        for (int i = x0; i < x1; i++)
            p[i] = cell;
    }
}

int
DrawContext::text(int x, int y, const char *s, unsigned char attr, unsigned char hot) const
{
    int x0 = x;
    bool on = false;
    for (; *s; s++) {
        if (hot && *s == '~') {
            on = !on;
            continue;
        }
        put(x++, y, (unsigned char)*s, on ? hot : attr);
    }
    return x - x0;
}

void
DrawContext::box(int x, int y, int w, int h, unsigned char attr, bool dbl) const
{
    unsigned char hz = dbl ? 0xcd : 0xc4, vt = dbl ? 0xba : 0xb3;
    fill(x + 1, y, w - 2, 1, hz, attr);
    fill(x + 1, y + h - 1, w - 2, 1, hz, attr);
    fill(x, y + 1, 1, h - 2, vt, attr);
    fill(x + w - 1, y + 1, 1, h - 2, vt, attr);
    put(x, y, dbl ? 0xc9 : 0xda, attr);
    put(x + w - 1, y, dbl ? 0xbb : 0xbf, attr);
    put(x, y + h - 1, dbl ? 0xc8 : 0xc0, attr);
    put(x + w - 1, y + h - 1, dbl ? 0xbc : 0xd9, attr);
}

void
DrawContext::shade(int x, int y, int w, int h) const
{
    int x0 = imax(x + ox, imax(clip.x, 0)), x1 = imin(x + ox + w, imin(clip.x + clip.w, scr::cols));
    int y0 = imax(y + oy, imax(clip.y, 0)), y1 = imin(y + oy + h, imin(clip.y + clip.h, scr::rows));
    for (int j = y0; j < y1; j++) {
        uint16_t *p = scr::buf + j * scr::cols;
        for (int i = x0; i < x1; i++)
            p[i] = (uint16_t)((p[i] & 0xff) | color::Shadow << 8);
    }
}

DrawContext
DrawContext::sub(const Rect &r) const
{
    int ax = ox + r.x, ay = oy + r.y;
    int x0 = imax(ax, clip.x), y0 = imax(ay, clip.y);
    int x1 = imin(ax + r.w, clip.x + clip.w), y1 = imin(ay + r.h, clip.y + clip.h);
    return DrawContext(ax, ay, Rect(x0, y0, imax(0, x1 - x0), imax(0, y1 - y0)));
}

} // namespace tui
