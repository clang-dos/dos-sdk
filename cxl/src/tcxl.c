/* TCXL-only functions, written over the CXL compatible core.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tcxl.h"
#include "cxl-local.h"

/* Opens a window, then centers it as cf asks (CNT_HOR, CNT_VER). */
WndT TcxlPopUp_(int cf, int sr, int sc, int er, int ec, int bt, int ba, int wa)
{
    int h = er - sr, w = ec - sc;

    if (cf & CNT_VER) {
        sr = (_vinfo.numrows - (h + 1)) / 2;
        er = sr + h;
    }
    if (cf & CNT_HOR) {
        sc = (_vinfo.numcols - (w + 1)) / 2;
        ec = sc + w;
    }
    return wopen(sr, sc, er, ec, bt, ba, wa);
}

/* Moves the active window to the center of the screen. */
IntT Wcenter(IntT cf)
{
    struct _wrec_t *w = _winfo.active;
    int sr, sc;

    if (!w)
        return _TcxlRet(_cxl_werr(W_NOACTIVE));
    sr = w->srow;
    sc = w->scol;
    if (cf & CNT_VER)
        sr = (_vinfo.numrows - (w->erow - w->srow + 1)) / 2;
    if (cf & CNT_HOR)
        sc = (_vinfo.numcols - (w->ecol - w->scol + 1)) / 2;
    return _TcxlRet(wmove(sr, sc));
}

/* Reports the last window error and ends the program. */
void Werror(void)
{
    int e = _winfo.errno;

    wcloseall();
    fprintf(stderr, "TCXL error %d: %s\n", e, werrmsg());
    exit(e ? e : 1);
}

/* Writes a string centered on the current row of the active window. */
IntT WputCen(ChrP ps)
{
    int r, c;

    if (_cxl_wbeg())
        return _TcxlRet(_winfo.errno);
    wreadcur(&r, &c);
    return _TcxlRet(wcenters(r, _winfo.active->attr, ps));
}

/* Writes a string right-justified on the current row, then moves to the
   start of the next row. */
IntT Wputrj(ChrP ps)
{
    int r, c, e;

    if (_cxl_wbeg())
        return _TcxlRet(_winfo.errno);
    wreadcur(&r, &c);
    e = wrjusts(r, _cxl_wcols(_winfo.active) - 1, _winfo.active->attr, ps);
    if (e == W_NOERROR && r + 1 < _cxl_wrows(_winfo.active))
        wgotoxy(r + 1, 0);
    return _TcxlRet(e);
}

/* Writes a string right-justified with its last character at (wr, wc). */
IntT Wprtrj(IntT wr, IntT wc, IntT at, ChrP ps)
{
    return _TcxlRet(wrjusts(wr, wc, at, ps));
}

/* Writes a string padded or cut to wd columns. */
void Wprtns(IntT wr, IntT wc, IntT at, ChrP ps, IntT wd)
{
    int len = strlen(ps), i;

    for (i = 0; i < wd; i++)
        wprintc(wr, wc + i, at, i < len ? (unsigned char)ps[i] : ' ');
}

/* Writes a string at (wr, wc) with word wrap. */
IntT Wprtsw(IntT wr, IntT wc, IntT at, ChrP ps)
{
    int old, e;

    if (_cxl_wbeg())
        return _TcxlRet(_winfo.errno);
    e = wgotoxy(wr, wc);
    if (e)
        return _TcxlRet(e);
    old = _winfo.active->attr;
    wtextattr(at);
    e = wputsw(ps);
    wtextattr(old);
    return _TcxlRet(e);
}

/* Formatted output at (wr, wc). */
IntT wprtf(IntT wr, IntT wc, IntT at, ChrP fs, ...)
{
    static char buf[512];
    va_list ap;

    va_start(ap, fs);
    vsnprintf(buf, sizeof buf, fs, ap);
    va_end(ap);
    return _TcxlRet(wprints(wr, wc, at, buf));
}

/* Writes a character nd times at (wr, wc). */
void WdupChrA(IntT wr, IntT wc, IntT at, IntT ch, IntT nd)
{
    while (nd-- > 0 && wprintc(wr, wc, at, ch) == W_NOERROR)
        wc++;
}

/* Writes a string nd times at (wr, wc). */
void WdupStrA(IntT wr, IntT wc, IntT at, ChrP ps, IntT nd)
{
    int len = strlen(ps);

    while (nd-- > 0 && wprints(wr, wc, at, ps) == W_NOERROR)
        wc += len;
}

/* Hides (s == 0) or shows (s > 0) the cursor, or only asks (s < 0).
   Returns 1 if the cursor was showing. */
IntT VcurHid(IntT s)
{
    int sl, el, was;

    getcursz(&sl, &el);
    was = !(sl & 0x20);
    if (s == 0 && was)
        hidecur();
    else if (s > 0 && !was)
        showcur();
    return was;
}

/* Sets an underline cursor (s == 0) or a block cursor. */
void VcurSet(IntT s)
{
    if (s)
        lgcursor();
    else
        smcursor();
}

static int tcxl_up;

void TcxlInit(void)
{
    if (tcxl_up)
        return;
    tcxl_up = 1;
    videoinit();
    atexit(TcxlTerm);
}

void TcxlTerm(void)
{
    if (!tcxl_up)
        return;
    tcxl_up = 0;
    wcloseall();
    showcur();
}

/* Defines an entry field. fm holds FLD_* bits, md an FDF_* mode (0, 1 and
   2 are accepted too), id tags the field for FldLoc(), ht is its help
   category. */
IntT EntFld(IntT wr, IntT wc, ChrP ps, ChrP fs, IntT fm, TagT id, IntT md,
            IfcpCP vf, TagT ht)
{
    int conv = 0, mode, e;

    if (fm & FLD_NUM)
        conv = '9';
    else if (fm & FLD_PAS)
        conv = 'P';
    else if (fm & FLD_MIX)
        conv = 'M';
    else if (fm & FLD_UPR)
        conv = 'U';
    else if (fm & FLD_LWR)
        conv = 'L';
    if (md & FDF_CND)
        mode = 2;
    else if (md & FDF_CHG)
        mode = 1;
    else if (md & FDF_INI)
        mode = 0;
    else
        mode = md >= 0 && md <= 2 ? md : 0;
    e = winpdef(wr, wc, ps, fs, conv, mode, vf, ht);
    if (e == W_NOERROR)
        _winfo.active->form->cfield->_tag = id;
    return _TcxlRet(e);
}

/* Finds a field of the active form by its tag. */
FdfP FldLoc(TagT id)
{
    struct _field_t *d;

    if (!_winfo.active || !_winfo.active->form)
        return NULL;
    for (d = _winfo.active->form->field; d; d = d->next)
        if (d->_tag == id)
            return d;
    return NULL;
}
