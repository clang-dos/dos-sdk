/* CXL compatible library: keyboard.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <ctype.h>
#include <dos.h>
#include "cxl-local.h"

struct _kbinfo_t _kbinfo;
int (*_cxl_helphook)(unsigned key);

unsigned _cxl_biosget(void)
{
    union REGS r;
    r.h.ah = 0;
    int86(0x16, &r, &r);
    return r.x.ax;
}

int _cxl_bioshit(void)
{
    union REGS r;
    r.h.ah = 1;
    int86(0x16, &r, &r);
    return !(r.x.flags & 0x40); /* ZF clear: a key is waiting */
}

static int buffered(void)
{
    return _kbinfo.kbuf != NULL;
}

static unsigned pop_buffer(void)
{
    struct _kbuf_t *k = _kbinfo.kbuf;
    unsigned x = k->xch;
    _kbinfo.kbuf = k->next;
    if (_kbinfo.kbuf)
        _kbinfo.kbuf->prev = NULL;
    free(k);
    return x;
}

/* One raw key: the CXL buffer, then the keyboard or mouse. */
static unsigned rawkey(void)
{
    unsigned k;
    int shown = 0;

    if (buffered()) {
        _kbinfo.source = 1;
        return pop_buffer();
    }
    for (;;) {
        if (_cxl_bioshit()) {
            k = _cxl_biosget();
            _kbinfo.source = 0;
            break;
        }
        if (_mouse) {
            if ((_mouse & MS_CURS) && !shown) {
                msshowcur();
                shown = 1;
            }
            k = _cxl_mousekey();
            if (k) {
                _kbinfo.source = 2;
                break;
            }
        }
        if (_kbinfo.kbloop)
            _kbinfo.kbloop();
        if (buffered()) {
            _kbinfo.source = 1;
            k = pop_buffer();
            break;
        }
    }
    if (shown)
        mshidecur();
    return k;
}

unsigned getxch(void)
{
    for (;;) {
        unsigned k = rawkey();
        struct _onkey_t *o;

        if (_cxl_helphook && _cxl_helphook(k))
            continue;
        for (o = _kbinfo.onkey; o; o = o->next)
            if (o->keycode == k)
                break;
        if (!o)
            return k;
        if (o->func)
            o->func();
        if (o->pass)
            return o->pass;
    }
}

int kbmhit(void)
{
    if (buffered() || _cxl_bioshit())
        return 1;
    if (_mouse) {
        unsigned k = _cxl_mousekey();
        if (k) {
            kbput(k);
            return 1;
        }
    }
    return 0;
}

int kbput(unsigned xch)
{
    struct _kbuf_t *k = malloc(sizeof *k), *t;
    if (!k)
        return 1;
    k->xch = xch;
    k->next = NULL;
    k->prev = NULL;
    if (!_kbinfo.kbuf) {
        _kbinfo.kbuf = k;
        return 0;
    }
    for (t = _kbinfo.kbuf; t->next; t = t->next)
        ;
    t->next = k;
    k->prev = t;
    return 0;
}

int kbputs(char *str)
{
    while (*str) {
        int c = (unsigned char)*str++;
        if (kbput(((unsigned)scancode(c) << 8) | c))
            return 1;
    }
    return 0;
}

void kbclear(void)
{
    while (_kbinfo.kbuf)
        pop_buffer();
}

void clearkeys(void)
{
    while (_cxl_bioshit())
        _cxl_biosget();
}

unsigned kbstat(void)
{
    union REGS r;
    r.h.ah = 2;
    int86(0x16, &r, &r);
    return r.h.al;
}

static void kbflag(unsigned char set, unsigned char clr)
{
    unsigned char __far *f = (unsigned char __far *)MK_FP(0x40, 0x17);
    *f = (*f | set) & ~clr;
    /* Poll the keyboard so the BIOS updates the LEDs. */
    _cxl_bioshit();
}

void capson(void)  { kbflag(CAPSLOCK, 0); }
void capsoff(void) { kbflag(0, CAPSLOCK); }
void numon(void)   { kbflag(NUMLOCK, 0); }
void numoff(void)  { kbflag(0, NUMLOCK); }

int setonkey(unsigned keycode, void (*func)(void), unsigned pass)
{
    struct _onkey_t *o;
    for (o = _kbinfo.onkey; o; o = o->next)
        if (o->keycode == keycode)
            break;
    if (!func) {
        if (o) {
            if (o->prev)
                o->prev->next = o->next;
            else
                _kbinfo.onkey = o->next;
            if (o->next)
                o->next->prev = o->prev;
            free(o);
        }
        return 0;
    }
    if (!o) {
        o = malloc(sizeof *o);
        if (!o)
            return 1;
        o->keycode = keycode;
        o->prev = NULL;
        o->next = _kbinfo.onkey;
        if (o->next)
            o->next->prev = o;
        _kbinfo.onkey = o;
    }
    o->func = func;
    o->pass = pass;
    return 0;
}

struct _onkey_t *chgonkey(struct _onkey_t *kblist)
{
    struct _onkey_t *old = _kbinfo.onkey;
    _kbinfo.onkey = kblist;
    return old;
}

void freonkey(void)
{
    while (_kbinfo.onkey) {
        struct _onkey_t *n = _kbinfo.onkey->next;
        free(_kbinfo.onkey);
        _kbinfo.onkey = n;
    }
}

int scancode(int ch)
{
    static const char row1[] = "1234567890-=";
    static const char row1s[] = "!@#$%^&*()_+";
    static const char row2[] = "qwertyuiop[]";
    static const char row2s[] = "QWERTYUIOP{}";
    static const char row3[] = "asdfghjkl;'`";
    static const char row3s[] = "ASDFGHJKL:\"~";
    static const char row4[] = "\\zxcvbnm,./";
    static const char row4s[] = "|ZXCVBNM<>?";
    const char *p;

    ch &= 0xff;
    if (ch == 0)
        return 0;
    if (ch >= 1 && ch <= 26 && ch != 8 && ch != 9 && ch != 13)
        return scancode(ch + 'a' - 1);
    switch (ch) {
    case 8: return 0x0e;
    case 9: return 0x0f;
    case 13: return 0x1c;
    case 27: return 0x01;
    case ' ': return 0x39;
    }
    if ((p = strchr(row1, ch)) != NULL) return 0x02 + (p - row1);
    if ((p = strchr(row1s, ch)) != NULL) return 0x02 + (p - row1s);
    if ((p = strchr(row2, ch)) != NULL) return 0x10 + (p - row2);
    if ((p = strchr(row2s, ch)) != NULL) return 0x10 + (p - row2s);
    if ((p = strchr(row3, ch)) != NULL) return 0x1e + (p - row3);
    if ((p = strchr(row3s, ch)) != NULL) return 0x1e + (p - row3s);
    if ((p = strchr(row4, ch)) != NULL) return 0x2b + (p - row4);
    if ((p = strchr(row4s, ch)) != NULL) return 0x2b + (p - row4s);
    return 0;
}

static unsigned long ticks(void)
{
    return *(volatile unsigned long __far *)MK_FP(0x40, 0x6c);
}

int waitkey(void)
{
    clearkeys();
    return getxch() & 0xff;
}

int waitkeyt(int duration)
{
    unsigned long start = ticks();
    clearkeys();
    while (!kbmhit()) {
        unsigned long now = ticks();
        if (now < start) /* midnight */
            start = now;
        if (now - start >= (unsigned)duration)
            return -1;
        if (_kbinfo.kbloop)
            _kbinfo.kbloop();
    }
    return getxch() & 0xff;
}

int getchf(char *valid, int defchar)
{
    for (;;) {
        int c = getxch() & 0xff;
        if (c == 0x1b)
            return 0;
        if (c == '\r' && defchar)
            return toupper(defchar);
        if (c) {
            const char *p;
            for (p = valid; *p; p++)
                if (toupper((unsigned char)*p) == toupper(c))
                    return toupper(c);
        }
    }
}

/* Screen (non-window) line input writes at the cursor. */
static void scr_tty(int ch, int attr)
{
    int r, c;
    _cxl_getcur(&r, &c);
    switch (ch) {
    case '\n':
        c = 0;
        r++;
        break;
    case '\r':
        c = 0;
        break;
    case '\b':
        if (c > 0)
            c--;
        else if (r > 0) {
            r--;
            c = _vinfo.numcols - 1;
        }
        break;
    default:
        _cxl_vput(r, c, CELL(ch, attr));
        if (++c >= _vinfo.numcols) {
            c = 0;
            r++;
        }
    }
    if (r >= _vinfo.numrows) {
        union REGS g;
        r = _vinfo.numrows - 1;
        g.x.ax = 0x0601;
        g.h.bh = attr;
        g.x.cx = 0;
        g.h.dh = r;
        g.h.dl = _vinfo.numcols - 1;
        int86(0x10, &g, &g);
    }
    _cxl_setcur(r, c);
}

static void scr_putat(int r, int c, int ch, int attr)
{
    _cxl_vput(r, c, CELL(ch, attr));
}

static void scr_io(struct _cxl_lineio *io)
{
    int r, c;
    _cxl_getcur(&r, &c);
    io->getpos = _cxl_getcur;
    io->setpos = _cxl_setcur;
    io->tty = scr_tty;
    io->putat = scr_putat;
    io->attr = _cxl_vget(r, c) >> 8;
}

int getns(char *str, int maxchars)
{
    struct _cxl_lineio io;
    scr_io(&io);
    return _cxl_strinput(&io, str, maxchars, 1);
}

int inputsf(char *str, char *fmt)
{
    struct _cxl_lineio io;
    scr_io(&io);
    return _cxl_lineinput(&io, str, fmt, 1, 1);
}
