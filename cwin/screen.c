/* Handles, the screen buffer, drawing, the keyboard and the mouse.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "cwin-local.h"
#include <stdlib.h>
#include <string.h>

/* ---- Handles: 1-based indexes into a table of objects. ---- */

static void **objs;
static unsigned nobjs;

void *
cw_obj(HANDLE h, int kind)
{
    if (!h || h > nobjs || !objs[h - 1])
        return 0;
    BYTE *o = (BYTE *)objs[h - 1];
    return *o == kind ? o : 0;
}

HANDLE
cw_newhandle(void *obj)
{
    unsigned i;
    for (i = 0; i < nobjs; i++)
        if (!objs[i])
            break;
    if (i == nobjs) {
        unsigned n = nobjs ? nobjs * 2 : 32;
        void **t = (void **)realloc(objs, n * sizeof(void *));
        if (!t)
            return 0;
        memset(t + nobjs, 0, (n - nobjs) * sizeof(void *));
        objs = t;
        nobjs = n;
    }
    objs[i] = obj;
    return (HANDLE)(i + 1);
}

void
cw_freehandle(HANDLE h)
{
    if (h && h <= nobjs)
        objs[h - 1] = 0;
}

/* ---- Small helpers. ---- */

char *
cw_strdup(const char *s)
{
    if (!s)
        s = "";
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d)
        memcpy(d, s, n);
    return d;
}

int cw_imin(int a, int b) { return a < b ? a : b; }
int cw_imax(int a, int b) { return a > b ? a : b; }
int cw_upper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

int
cw_mnemonic(const char *s)
{
    for (; s && *s; s++)
        if (*s == '&') {
            if (s[1] == '&') {
                s++;
                continue;
            }
            return s[1] ? cw_upper((unsigned char)s[1]) : 0;
        }
    return 0;
}

int
cw_textlen(const char *s)
{
    int n = 0;
    for (; s && *s && *s != '\t'; s++) {
        if (*s == '&') {
            if (s[1] != '&')
                continue;
            s++;
        }
        n++;
    }
    return n;
}

void
cw_rect(RECT *r, int x, int y, int w, int h)
{
    r->left = x;
    r->top = y;
    r->right = x + w;
    r->bottom = y + h;
}

void
cw_intersect(RECT *r, const RECT *a)
{
    r->left = cw_imax(r->left, a->left);
    r->top = cw_imax(r->top, a->top);
    r->right = cw_imin(r->right, a->right);
    r->bottom = cw_imin(r->bottom, a->bottom);
    if (r->right < r->left)
        r->right = r->left;
    if (r->bottom < r->top)
        r->bottom = r->top;
}

/* ---- The screen. ---- */

int cw_cols = 80, cw_rows = 25;
uint16_t *cw_buf;
BYTE cw_mouse;
int cw_mx, cw_my;
BYTE cw_inited;

/* Text attributes for the COLOR_ indexes, in the style of the
 * character-mode applications: gray menus and dialogs, blue windows. */
BYTE cw_sys[CW_NCOLORS] = {
    0x70, /* SCROLLBAR */
    0x71, /* BACKGROUND (the desktop) */
    0x71, /* ACTIVECAPTION */
    0x17, /* INACTIVECAPTION */
    0x70, /* MENU */
    0x17, /* WINDOW */
    0x17, /* WINDOWFRAME */
    0x70, /* MENUTEXT */
    0x17, /* WINDOWTEXT */
    0x71, /* CAPTIONTEXT */
    0x1f, /* ACTIVEBORDER */
    0x17, /* INACTIVEBORDER */
    0x71, /* APPWORKSPACE */
    0x07, /* HIGHLIGHT */
    0x07, /* HIGHLIGHTTEXT */
    0x70, /* BTNFACE */
    0x08, /* BTNSHADOW */
    0x78, /* GRAYTEXT */
    0x70, /* BTNTEXT */
    0x7f, /* MENUHOT */
    0x07, /* MENUSEL */
    0x0f, /* MENUSELHOT */
    0x70, /* DIALOG */
    0x7f, /* DIALOGHOT */
    0x7f, /* BTNFOCUS */
    0x70, /* EDIT */
    0x30, /* STATUS */
    0x08, /* SHADOW */
};

static uint16_t vseg;
static uint16_t *shown;      /* what is in video memory */
static uint16_t *saved;      /* the screen before cw_init */
static uint16_t savedCursor, savedShape;
static BYTE cursorOn = 1;
static BYTE mbuttons;
static uint16_t lastClick;
static int lastX = -1, lastY;
static BYTE altDown, altAlone;

typedef volatile uint8_t __far *bptr;
typedef volatile uint16_t __far *wptr;

static uint8_t bda8(unsigned off) { return *(bptr)(((uint32_t)0x40 << 16) | off); }
static uint16_t bda16(unsigned off) { return *(wptr)(((uint32_t)0x40 << 16) | off); }

static uint16_t
int10(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx)
{
    __asm__ __volatile__("pushw %%bp\n\tint $0x10\n\tpopw %%bp"
                         : "+a"(ax), "+b"(bx), "+c"(cx), "+d"(dx)
                         :
                         : "si", "di", "memory", "cc");
    return ax;
}

static void
int33(uint16_t *ax, uint16_t *bx, uint16_t *cx, uint16_t *dx)
{
    uint16_t a = *ax, b = *bx, c = *cx, d = *dx;
    __asm__ __volatile__("pushw %%bp\n\tint $0x33\n\tpopw %%bp"
                         : "+a"(a), "+b"(b), "+c"(c), "+d"(d)
                         :
                         : "si", "di", "memory", "cc");
    *ax = a;
    *bx = b;
    *cx = c;
    *dx = d;
}

static void
mouse(uint16_t ax, uint16_t cx, uint16_t dx)
{
    uint16_t bx = 0;
    int33(&ax, &bx, &cx, &dx);
}

static wptr video(unsigned i) { return (wptr)(((uint32_t)vseg << 16) | (uint16_t)(i * 2)); }

void
cw_init(void)
{
    if (cw_inited)
        return;
    uint8_t mode = bda8(0x49) & 0x7f;
    if (mode != 2 && mode != 3 && mode != 7) {
        int10(0x0003, 0, 0, 0);
        mode = 3;
    }
    cw_cols = bda16(0x4a);
    if (cw_cols < 40 || cw_cols > 132)
        cw_cols = 80;
    uint8_t last = bda8(0x84);
    cw_rows = (last >= 24 && last < 100) ? last + 1 : 25;
    vseg = (mode == 7 ? 0xb000 : 0xb800) + (bda16(0x4e) >> 4);
    unsigned n = (unsigned)cw_cols * cw_rows;
    cw_buf = (uint16_t *)calloc(n, 2);
    shown = (uint16_t *)malloc(n * 2);
    saved = (uint16_t *)malloc(n * 2);
    for (unsigned i = 0; i < n; i++) {
        uint16_t c = *video(i);
        if (saved)
            saved[i] = c;
        if (shown)
            shown[i] = c;
    }
    savedCursor = bda16(0x50 + 2 * bda8(0x62));
    savedShape = bda16(0x60);
    /* Bright backgrounds instead of blinking. */
    int10(0x1003, 0, 0, 0);
    wptr vec = (wptr)(uint32_t)(0x33 * 4);
    if (vec[0] || vec[1]) {
        uint16_t ax = 0, bx = 0, cx = 0, dx = 0;
        int33(&ax, &bx, &cx, &dx);
        if (ax == 0xffff) {
            cw_mouse = 1;
            mouse(7, 0, (uint16_t)((cw_cols - 1) * 8));
            mouse(8, 0, (uint16_t)((cw_rows - 1) * 8));
            mouse(4, (uint16_t)(cw_cols / 2 * 8), (uint16_t)(cw_rows / 2 * 8));
            cw_mx = cw_cols / 2;
            cw_my = cw_rows / 2;
        }
    }
    cw_inited = 1;
}

void
CwShutdown(void)
{
    if (!cw_inited)
        return;
    unsigned n = (unsigned)cw_cols * cw_rows;
    if (saved)
        for (unsigned i = 0; i < n; i++)
            *video(i) = saved[i];
    int10(0x1003, 1, 0, 0);
    int10(0x0100, 0, savedShape, 0);
    int10(0x0200, (uint16_t)bda8(0x62) << 8, 0, savedCursor);
    free(saved);
    free(shown);
    saved = shown = 0;
    cw_inited = 0;
}

int CwScreenCols(void) { cw_setup(); return cw_cols; }
int CwScreenRows(void) { cw_setup(); return cw_rows; }

void
cw_flush(int curx, int cury)
{
    unsigned n = (unsigned)cw_cols * cw_rows;
    unsigned m = cw_mouse && cw_mx >= 0 && cw_mx < cw_cols && cw_my >= 0 && cw_my < cw_rows
                     ? (unsigned)(cw_my * cw_cols + cw_mx)
                     : n;
    /* Only the cells that changed, which is faster and makes less snow
     * on a CGA. */
    for (unsigned i = 0; i < n; i++) {
        uint16_t c = cw_buf[i];
        if (i == m)
            c ^= 0x7700;
        if (!shown || shown[i] != c) {
            *video(i) = c;
            if (shown)
                shown[i] = c;
        }
    }
    if (curx >= 0 && curx < cw_cols && cury >= 0 && cury < cw_rows) {
        int10(0x0200, (uint16_t)bda8(0x62) << 8, 0, (uint16_t)(cury << 8 | curx));
        if (!cursorOn) {
            int10(0x0100, 0, savedShape && savedShape != 0x2000 ? savedShape : 0x0607, 0);
            cursorOn = 1;
        }
    } else if (cursorOn) {
        int10(0x0100, 0, 0x2000, 0);
        cursorOn = 0;
    }
}

uint16_t cw_ticks(void) { return bda16(0x6c); }

static BYTE
keyReady(void)
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

BYTE cw_shiftflags(void) { return (BYTE)int16(0x0200); }

int
cw_poll(struct cwev *e)
{
    memset(e, 0, sizeof *e);
    BYTE sh = cw_shiftflags();
    e->shift = sh;
    if (keyReady()) {
        e->type = EV_KEY;
        e->key = int16(0x0000);
        if ((e->key & 0xff) == 0xe0)
            e->key &= 0xff00;
        altAlone = 0;
        return 1;
    }
    /* Alt pressed and let go with no key in between: the menu bar. */
    if (sh & 0x08) {
        if (!altDown)
            altAlone = 1;
        altDown = 1;
    } else if (altDown) {
        altDown = 0;
        if (altAlone) {
            altAlone = 0;
            e->type = EV_ALTTAP;
            return 1;
        }
    }
    if (!cw_mouse)
        return 0;
    uint16_t ax = 3, bx = 0, cx = 0, dx = 0;
    int33(&ax, &bx, &cx, &dx);
    int x = cx / 8, y = dx / 8;
    BYTE b = bx & 3;
    e->x = x;
    e->y = y;
    e->buttons = b;
    if (b && !mbuttons) {
        e->type = EV_DOWN;
        uint16_t now = cw_ticks();
        e->dbl = x == lastX && y == lastY && (uint16_t)(now - lastClick) <= 8;
        lastClick = now;
        lastX = e->dbl ? -1 : x;
        lastY = y;
        altAlone = 0;
    } else if (!b && mbuttons) {
        e->type = EV_UP;
        e->buttons = mbuttons;
    } else if (x != cw_mx || y != cw_my) {
        e->type = EV_MOVE;
    }
    mbuttons = b;
    cw_mx = x;
    cw_my = y;
    return e->type != EV_NONE;
}

void
cw_idle(void)
{
    /* The DOS idle interrupt, then the multiplex call that gives the time
     * slice away under Windows and in emulators. */
    __asm__ __volatile__("int $0x28" : : : "memory");
    uint16_t ax = 0x1680;
    __asm__ __volatile__("int $0x2f" : "+a"(ax) : : "memory", "cc");
}

int
GetSystemMetrics(int index)
{
    cw_setup();
    switch (index) {
    case SM_CXSCREEN:
        return cw_cols;
    case SM_CYSCREEN:
        return cw_rows;
    case SM_MOUSEPRESENT:
        return cw_mouse;
    }
    return 0;
}

void
MessageBeep(UINT type)
{
    (void)type;
    /* The BIOS teletype beeps for a BEL. */
    int10(0x0e07, 0, 0, 0);
}

DWORD
GetSysColor(int index)
{
    return index >= 0 && index < CW_NCOLORS ? cw_sys[index] : 0;
}

void
SetSysColors(int n, const int *indices, const COLORREF *colors)
{
    for (int i = 0; i < n; i++)
        if (indices[i] >= 0 && indices[i] < CW_NCOLORS)
            cw_sys[indices[i]] = (BYTE)colors[i];
    cw_dirty = 1;
}

/* ---- Drawing into the buffer, clipped (screen cells). ---- */

void
cw_put(const RECT *c, int x, int y, BYTE ch, BYTE attr)
{
    if (x < c->left || x >= c->right || y < c->top || y >= c->bottom || x < 0 || y < 0 ||
        x >= cw_cols || y >= cw_rows)
        return;
    cw_buf[y * cw_cols + x] = (uint16_t)(attr << 8 | ch);
}

void
cw_fill(const RECT *c, int x, int y, int w, int h, BYTE ch, BYTE attr)
{
    int x0 = cw_imax(x, cw_imax(c->left, 0)), x1 = cw_imin(x + w, cw_imin(c->right, cw_cols));
    int y0 = cw_imax(y, cw_imax(c->top, 0)), y1 = cw_imin(y + h, cw_imin(c->bottom, cw_rows));
    uint16_t cell = (uint16_t)(attr << 8 | ch);
    for (int j = y0; j < y1; j++) {
        uint16_t *p = cw_buf + j * cw_cols;
        for (int i = x0; i < x1; i++)
            p[i] = cell;
    }
}

void
cw_shade(const RECT *c, int x, int y, int w, int h)
{
    int x0 = cw_imax(x, cw_imax(c->left, 0)), x1 = cw_imin(x + w, cw_imin(c->right, cw_cols));
    int y0 = cw_imax(y, cw_imax(c->top, 0)), y1 = cw_imin(y + h, cw_imin(c->bottom, cw_rows));
    for (int j = y0; j < y1; j++) {
        uint16_t *p = cw_buf + j * cw_cols;
        for (int i = x0; i < x1; i++)
            p[i] = (uint16_t)((p[i] & 0xff) | cw_sys[COLOR_SHADOW] << 8);
    }
}

/* Text up to n characters (or the end, or a tab); with hot != 0 the
 * letter after a & is in the hot color and the &s are not shown. Returns
 * the width. */
int
cw_text(const RECT *c, int x, int y, const char *s, int n, BYTE attr, BYTE hot)
{
    int x0 = x;
    for (int i = 0; s && s[i] && s[i] != '\t' && (n < 0 || i < n); i++) {
        BYTE a = attr;
        if (hot && s[i] == '&') {
            if (s[i + 1] != '&') {
                if (s[i + 1])
                    a = hot;
                i++;
                if (!s[i] || (n >= 0 && i >= n))
                    break;
            } else {
                i++;
            }
        }
        cw_put(c, x++, y, (BYTE)s[i], a);
    }
    return x - x0;
}

void
cw_box(const RECT *c, int x, int y, int w, int h, BYTE attr, BOOL dbl)
{
    BYTE hz = dbl ? 0xcd : 0xc4, vt = dbl ? 0xba : 0xb3;
    cw_fill(c, x + 1, y, w - 2, 1, hz, attr);
    cw_fill(c, x + 1, y + h - 1, w - 2, 1, hz, attr);
    cw_fill(c, x, y + 1, 1, h - 2, vt, attr);
    cw_fill(c, x + w - 1, y + 1, 1, h - 2, vt, attr);
    cw_put(c, x, y, dbl ? 0xc9 : 0xda, attr);
    cw_put(c, x + w - 1, y, dbl ? 0xbb : 0xbf, attr);
    cw_put(c, x, y + h - 1, dbl ? 0xc8 : 0xc0, attr);
    cw_put(c, x + w - 1, y + h - 1, dbl ? 0xbc : 0xd9, attr);
}
