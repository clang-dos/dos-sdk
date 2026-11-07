/* CXL compatible library: window keyboard input.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include "cxl-local.h"

#define W (_winfo.active)

static void w_getpos(int *r, int *c)
{
    *r = W->row;
    *c = W->column;
}

static void w_setpos(int r, int c)
{
    W->row = r;
    W->column = c;
    _cxl_wsyncur();
}

static void w_tty(int ch, int attr)
{
    int a = W->attr;
    W->attr = attr;
    _cxl_wtty(ch);
    W->attr = a;
    _cxl_wsyncur();
}

static void w_putat(int r, int c, int ch, int attr)
{
    if (r >= 0 && c >= 0 && r < _cxl_wrows(W) && c < _cxl_wcols(W))
        _cxl_vput(_cxl_wrow0(W) + r, _cxl_wcol0(W) + c, CELL(ch, attr));
}

static void w_io(struct _cxl_lineio *io)
{
    io->getpos = w_getpos;
    io->setpos = w_setpos;
    io->tty = w_tty;
    io->putat = w_putat;
    io->attr = W->attr;
}

int wgetc(void)
{
    int c;
    if (_cxl_wbeg())
        return 0;
    _cxl_wsyncur();
    c = getxch() & 0xff;
    if (c >= ' ')
        w_tty(c, W->attr);
    _cxl_werr(W_NOERROR);
    return c;
}

int wgetchf(char *valid, int defchar)
{
    if (_cxl_wbeg())
        return 0;
    _cxl_wsyncur();
    for (;;) {
        int c = getxch() & 0xff;
        const char *p;
        if (c == 0x1b && _winfo.esc) {
            _cxl_werr(W_ESCPRESS);
            return 0;
        }
        if (c == '\r' && defchar)
            c = defchar;
        for (p = valid; *p && c; p++)
            if (toupper((unsigned char)*p) == toupper(c)) {
                c = toupper(c);
                w_tty(c, W->attr);
                _cxl_werr(W_NOERROR);
                return c;
            }
    }
}

int wgetyn(int cdefault)
{
    int c;
    if (_cxl_wbeg())
        return 0;
    c = wgetchf("YN", cdefault ? 'Y' : 'N');
    if (!c)
        return 0;
    /* wgetchf echoed the letter; finish the word. */
    _cxl_wttys(c == 'Y' ? "es" : "o", 0);
    _cxl_wsyncur();
    return c;
}

int wgetns(char *str, int maxchars)
{
    struct _cxl_lineio io;
    if (_cxl_wbeg())
        return _winfo.errno;
    w_io(&io);
    _cxl_wsyncur();
    if (_cxl_strinput(&io, str, maxchars, _winfo.esc))
        return _cxl_werr(W_ESCPRESS);
    return _cxl_werr(W_NOERROR);
}

int wgets(char *str)
{
    struct _cxl_lineio io;
    if (_cxl_wbeg())
        return _winfo.errno;
    w_io(&io);
    _cxl_wsyncur();
    _cxl_strinput(&io, str, -1, 0);
    return _cxl_werr(W_NOERROR);
}

int winputsf(char *str, char *fmt)
{
    struct _cxl_lineio io;
    int r;
    if (_cxl_wbeg())
        return _winfo.errno;
    w_io(&io);
    _cxl_wsyncur();
    r = _cxl_lineinput(&io, str, fmt, _winfo.esc, 1);
    return _cxl_werr(r == 1 ? W_ESCPRESS : r == 2 ? W_INVFORMT : W_NOERROR);
}

int wscanf(const char *format, ...)
{
    char line[256];
    struct _cxl_lineio io;
    va_list ap;
    if (_cxl_wbeg())
        return _winfo.errno;
    w_io(&io);
    _cxl_wsyncur();
    _cxl_strinput(&io, line, sizeof line - 1, 0);
    va_start(ap, format);
    vsscanf(line, format, ap);
    va_end(ap);
    return _cxl_werr(W_NOERROR);
}
