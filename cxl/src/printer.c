/* CXL compatible library: printer output through DOS (PRN).
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdarg.h>
#include <stdio.h>
#include "cxl-local.h"
#include "cxlprn.h"

void lprintc(int ch)
{
    union REGS r;
    r.h.ah = 5;
    r.h.dl = ch;
    intdos(&r, &r);
}

void lprints(char *str)
{
    while (*str)
        lprintc((unsigned char)*str++);
}

void lcrlf(void)
{
    lprintc('\r');
    lprintc('\n');
}

void lprintf(const char *format, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, sizeof buf, format, ap);
    va_end(ap);
    lprints(buf);
}

void lprintns(char *str, int width)
{
    while (width-- > 0)
        lprintc(*str ? (unsigned char)*str++ : ' ');
}

/* Bold by striking the line again after a carriage return. */
void lprintsb(char *str, int reps)
{
    int i;
    lprints(str);
    for (i = 1; i < reps; i++) {
        lprintc('\r');
        lprints(str);
    }
}

void lprintsu(char *str)
{
    size_t i, n = strlen(str);
    lprints(str);
    lprintc('\r');
    for (i = 0; i < n; i++)
        lprintc('_');
}

void windump(int srow, int scol, int erow, int ecol)
{
    int r, c;
    for (r = srow; r <= erow; r++) {
        for (c = scol; c <= ecol; c++) {
            int ch = _cxl_vget(r, c) & 0xff;
            lprintc(ch < ' ' ? ' ' : ch);
        }
        lcrlf();
    }
}

void scrndump(void)
{
    windump(0, 0, _vinfo.numrows - 1, _vinfo.numcols - 1);
}

int wdump(void)
{
    struct _wrec_t *w = _winfo.active;
    if (_cxl_wbeg())
        return _winfo.errno;
    windump(w->srow, w->scol, w->erow, w->ecol);
    return _cxl_werr(W_NOERROR);
}
