/* CXL compatible library: Microsoft mouse driver (INT 33h) interface.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <dos.h>
#include "cxl-local.h"

int _mouse = MS_NONE;
int _cxl_mrow = -1, _cxl_mcol = -1;
static int present;

static void m33(union REGS *r)
{
    int86(0x33, r, r);
}

int msinit(void)
{
    union REGS r;
    void(__interrupt __far * v)() = _dos_getvect(0x33);
    present = 0;
    _mouse = MS_NONE;
    if (!v)
        return 0;
    r.x.ax = 0;
    m33(&r);
    if (r.x.ax != 0xffff)
        return 0;
    present = 1;
    _mouse = MS_KEYS;
    mshbounds(0, _vinfo.numcols - 1);
    msvbounds(0, _vinfo.numrows - 1);
    return 1;
}

void mssupport(int type)
{
    if (present)
        _mouse = type;
}

void msshowcur(void)
{
    union REGS r;
    if (!present)
        return;
    r.x.ax = 1;
    m33(&r);
}

void mshidecur(void)
{
    union REGS r;
    if (!present)
        return;
    r.x.ax = 2;
    m33(&r);
}

void msstatus(int *bstat, int *row, int *col)
{
    union REGS r;
    *bstat = *row = *col = 0;
    if (!present)
        return;
    r.x.ax = 3;
    m33(&r);
    *bstat = r.x.bx;
    *row = r.x.dx / 8;
    *col = r.x.cx / 8;
}

void msgotoxy(int row, int col)
{
    union REGS r;
    if (!present)
        return;
    r.x.ax = 4;
    r.x.cx = col * 8;
    r.x.dx = row * 8;
    m33(&r);
}

static void button(int fn, int button, int *bstat, int *bcount, int *row,
                   int *col)
{
    union REGS r;
    *bstat = *bcount = *row = *col = 0;
    if (!present)
        return;
    r.x.ax = fn;
    r.x.bx = button;
    m33(&r);
    *bstat = r.x.ax;
    *bcount = r.x.bx;
    *row = r.x.dx / 8;
    *col = r.x.cx / 8;
}

void msbpress(int b, int *bstat, int *bcount, int *row, int *col)
{
    button(5, b, bstat, bcount, row, col);
}

void msbreles(int b, int *bstat, int *bcount, int *row, int *col)
{
    button(6, b, bstat, bcount, row, col);
}

void msbclear(void)
{
    int s, c, r, k, b;
    for (b = 0; b < 3; b++) {
        msbpress(b, &s, &c, &r, &k);
        msbreles(b, &s, &c, &r, &k);
    }
}

void mshbounds(int leftcol, int rightcol)
{
    union REGS r;
    if (!present)
        return;
    r.x.ax = 7;
    r.x.cx = leftcol * 8;
    r.x.dx = rightcol * 8;
    m33(&r);
}

void msvbounds(int toprow, int botrow)
{
    union REGS r;
    if (!present)
        return;
    r.x.ax = 8;
    r.x.cx = toprow * 8;
    r.x.dx = botrow * 8;
    m33(&r);
}

void mscursor(unsigned curtype, unsigned smask, unsigned cmask)
{
    union REGS r;
    if (!present)
        return;
    r.x.ax = 10;
    r.x.bx = curtype;
    r.x.cx = smask;
    r.x.dx = cmask;
    m33(&r);
}

/* Mickey counts since the last call. */
void msmotion(int *rowcount, int *colcount)
{
    union REGS r;
    *rowcount = *colcount = 0;
    if (!present)
        return;
    r.x.ax = 11;
    m33(&r);
    *rowcount = (short)r.x.dx;
    *colcount = (short)r.x.cx;
}

void msspeed(int xratio, int yratio)
{
    union REGS r;
    if (!present)
        return;
    r.x.ax = 15;
    r.x.cx = xratio;
    r.x.dx = yratio;
    m33(&r);
}

void mscondoff(int srow, int scol, int erow, int ecol)
{
    union REGS r;
    if (!present)
        return;
    r.x.ax = 16;
    r.x.cx = scol * 8;
    r.x.dx = srow * 8;
    r.x.si = ecol * 8;
    r.x.di = erow * 8;
    m33(&r);
}

/* Mouse key emulation while waiting for a key: movement gives arrow
 * keys, the left button Enter and the right button Esc. */
unsigned _cxl_mousekey(void)
{
    static int accx, accy;
    int s, c, row, col, dy, dx;

    if (!present || _mouse == MS_NONE)
        return 0;
    msbpress(0, &s, &c, &row, &col);
    if (c) {
        _cxl_mrow = row;
        _cxl_mcol = col;
        return 0x1c0d;
    }
    msbpress(1, &s, &c, &row, &col);
    if (c)
        return 0x011b;
    if (!(_mouse & MS_KEYS))
        return 0;
    msmotion(&dy, &dx);
    accx += dx;
    accy += dy;
    if (accy <= -16) {
        accy = accx = 0;
        return 0x4800;
    }
    if (accy >= 16) {
        accy = accx = 0;
        return 0x5000;
    }
    if (accx <= -16) {
        accy = accx = 0;
        return 0x4b00;
    }
    if (accx >= 16) {
        accy = accx = 0;
        return 0x4d00;
    }
    return 0;
}
