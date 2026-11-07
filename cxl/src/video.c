/* CXL compatible library: low level video.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <dos.h>
#include <stdio.h>
#include "cxl-local.h"

struct _vinfo_t _vinfo = {
    0xb800, V_CGA, 25, 80, 8, 8, 0, 0, 0, 0, 1, 0
};

static unsigned __far *cellp(int row, int col)
{
    return (unsigned __far *)MK_FP(_vinfo.videoseg,
                                   ((unsigned)row * _vinfo.numcols + col) * 2);
}

/* Wait for horizontal retrace on a CGA, the only time video memory can be
 * touched without snow. */
static void retrace(void)
{
    while (inportb(0x3da) & 1)
        ;
    while (!(inportb(0x3da) & 1))
        ;
}

static void bios_cursor(int row, int col)
{
    union REGS r;
    r.h.ah = 2;
    r.h.bh = 0;
    r.h.dh = row;
    r.h.dl = col;
    int86(0x10, &r, &r);
}

static unsigned bios_cursor_get(void)
{
    union REGS r;
    r.h.ah = 3;
    r.h.bh = 0;
    int86(0x10, &r, &r);
    return r.x.dx;
}

static void bios_put(unsigned cell)
{
    union REGS r;
    r.h.ah = 9;
    r.h.al = cell & 0xff;
    r.h.bh = 0;
    r.h.bl = cell >> 8;
    r.x.cx = 1;
    int86(0x10, &r, &r);
}

static unsigned bios_get(void)
{
    union REGS r;
    r.h.ah = 8;
    r.h.bh = 0;
    int86(0x10, &r, &r);
    return r.x.ax;
}

int mapattr(int attr)
{
    int fg = attr & 7, bg = (attr >> 4) & 7;
    int hi = attr & 8, bl = attr & 0x80;

    if (bg == 0)
        return (fg == 0 ? 0 : (fg == BLUE ? 1 : 7)) | hi | bl;
    if (fg == 0 || bg == CYAN || bg == BROWN || bg == LIGHTGRAY)
        return 0x70 | bl;
    return 7 | hi | bl;
}

int _cxl_attr(int attr)
{
    return _vinfo.mapattr ? mapattr(attr) : (attr & 0xff);
}

int revsattr(int attr)
{
    return ((attr & 0x07) << 4) | ((attr >> 4) & 0x07) | (attr & 0x88);
}

static unsigned mapcell(unsigned cell)
{
    if (_vinfo.mapattr)
        cell = (cell & 0xff) | ((unsigned)mapattr(cell >> 8) << 8);
    return cell;
}

unsigned _cxl_vget(int row, int col)
{
    unsigned v;
    if (_vinfo.usebios) {
        unsigned cur = bios_cursor_get();
        bios_cursor(row, col);
        v = bios_get();
        bios_cursor(cur >> 8, cur & 0xff);
        return v;
    }
    if (_vinfo.cgasnow)
        retrace();
    return *cellp(row, col);
}

void _cxl_vput(int row, int col, unsigned cell)
{
    if (row < 0 || col < 0 || row >= _vinfo.numrows || col >= _vinfo.numcols)
        return;
    cell = mapcell(cell);
    if (_vinfo.usebios) {
        unsigned cur = bios_cursor_get();
        bios_cursor(row, col);
        bios_put(cell);
        bios_cursor(cur >> 8, cur & 0xff);
        return;
    }
    if (_vinfo.cgasnow)
        retrace();
    *cellp(row, col) = cell;
}

void _cxl_vgetn(int row, int col, unsigned *dst, int n)
{
    int i;
    if (_vinfo.usebios || _vinfo.cgasnow) {
        for (i = 0; i < n; i++)
            dst[i] = _cxl_vget(row, col + i);
        return;
    }
    {
        unsigned __far *p = cellp(row, col);
        for (i = 0; i < n; i++)
            dst[i] = p[i];
    }
}

/* Cells written back from a save buffer are already mapped. */
void _cxl_vputraw(int row, int col, const unsigned *src, int n)
{
    int i;
    if (_vinfo.usebios || _vinfo.cgasnow) {
        unsigned char m = _vinfo.mapattr;
        _vinfo.mapattr = 0;
        for (i = 0; i < n; i++)
            _cxl_vput(row, col + i, src[i]);
        _vinfo.mapattr = m;
        return;
    }
    {
        unsigned __far *p = cellp(row, col);
        for (i = 0; i < n; i++)
            p[i] = src[i];
    }
}

void _cxl_vputn(int row, int col, const unsigned *src, int n)
{
    int i;
    for (i = 0; i < n; i++)
        _cxl_vput(row, col + i, src[i]);
}

void _cxl_vfill(int srow, int scol, int erow, int ecol, unsigned cell)
{
    int r, c;
    if (srow < 0)
        srow = 0;
    if (scol < 0)
        scol = 0;
    if (erow >= _vinfo.numrows)
        erow = _vinfo.numrows - 1;
    if (ecol >= _vinfo.numcols)
        ecol = _vinfo.numcols - 1;
    if (!_vinfo.usebios && !_vinfo.cgasnow) {
        cell = mapcell(cell);
        for (r = srow; r <= erow; r++) {
            unsigned __far *p = cellp(r, scol);
            for (c = scol; c <= ecol; c++)
                *p++ = cell;
        }
        return;
    }
    for (r = srow; r <= erow; r++)
        for (c = scol; c <= ecol; c++)
            _cxl_vput(r, c, cell);
}

int *_cxl_save(int srow, int scol, int erow, int ecol)
{
    int w, h, r;
    int *buf;
    if (srow < 0)
        srow = 0;
    if (scol < 0)
        scol = 0;
    if (erow >= _vinfo.numrows)
        erow = _vinfo.numrows - 1;
    if (ecol >= _vinfo.numcols)
        ecol = _vinfo.numcols - 1;
    w = ecol - scol + 1;
    h = erow - srow + 1;
    if (w <= 0 || h <= 0) {
        buf = malloc(4 * sizeof(int));
        if (buf) {
            buf[0] = 1;
            buf[1] = 1;
            buf[2] = 0;
            buf[3] = 0;
        }
        return buf;
    }
    buf = malloc((4 + (size_t)w * h) * sizeof(int));
    if (!buf)
        return NULL;
    buf[0] = srow;
    buf[1] = scol;
    buf[2] = erow;
    buf[3] = ecol;
    for (r = 0; r < h; r++)
        _cxl_vgetn(srow + r, scol, (unsigned *)buf + 4 + r * w, w);
    return buf;
}

void _cxl_restore_at(const int *buf, int srow, int scol)
{
    int w, h, r;
    if (!buf)
        return;
    w = buf[3] - buf[1] + 1;
    h = buf[2] - buf[0] + 1;
    if (w <= 0 || h <= 0)
        return;
    for (r = 0; r < h; r++) {
        int row = srow + r, col = scol, n = w, skip = 0;
        if (row < 0 || row >= _vinfo.numrows)
            continue;
        if (col < 0) {
            skip = -col;
            n -= skip;
            col = 0;
        }
        if (col + n > _vinfo.numcols)
            n = _vinfo.numcols - col;
        if (n > 0)
            _cxl_vputraw(row, col, (const unsigned *)buf + 4 + r * w + skip, n);
    }
}

void _cxl_restore(const int *buf)
{
    if (buf)
        _cxl_restore_at(buf, buf[0], buf[1]);
}

void _cxl_setcur(int row, int col)
{
    bios_cursor(row, col);
}

void _cxl_getcur(int *row, int *col)
{
    unsigned dx = bios_cursor_get();
    *row = dx >> 8;
    *col = dx & 0xff;
}

void gotoxy_(int row, int col)
{
    bios_cursor(row, col);
}

void readcur(int *row, int *col)
{
    _cxl_getcur(row, col);
}

void printc(int row, int col, int attr, int ch)
{
    _cxl_vput(row, col, CELL(ch, attr));
}

void prints(int row, int col, int attr, char *str)
{
    while (*str && col < _vinfo.numcols)
        _cxl_vput(row, col++, CELL(*str++, attr));
}

void fill_(int srow, int scol, int erow, int ecol, int ch, int attr)
{
    _cxl_vfill(srow, scol, erow, ecol, CELL(ch, attr));
}

void cclrscrn(int attr)
{
    _cxl_vfill(0, 0, _vinfo.numrows - 1, _vinfo.numcols - 1, CELL(' ', attr));
    gotoxy_(0, 0);
}

static int attr_at_cursor(void)
{
    int r, c;
    _cxl_getcur(&r, &c);
    return _cxl_vget(r, c) >> 8;
}

void clrscrn(void)
{
    cclrscrn(attr_at_cursor());
}

void clrwin(int srow, int scol, int erow, int ecol)
{
    _cxl_vfill(srow, scol, erow, ecol, CELL(' ', attr_at_cursor()));
}

void clreol_(void)
{
    int r, c;
    _cxl_getcur(&r, &c);
    _cxl_vfill(r, c, r, _vinfo.numcols - 1, CELL(' ', _cxl_vget(r, c) >> 8));
}

void spc(int num)
{
    int r, c, a;
    _cxl_getcur(&r, &c);
    a = _cxl_vget(r, c) >> 8;
    while (num-- > 0) {
        _cxl_vput(r, c, CELL(' ', a));
        if (++c >= _vinfo.numcols) {
            c = 0;
            if (++r >= _vinfo.numrows)
                r = _vinfo.numrows - 1;
        }
    }
    gotoxy_(r, c);
}

void putchat(int ch, int attr)
{
    bios_put(CELL(ch, _cxl_attr(attr)));
}

unsigned readchat(void)
{
    return bios_get();
}

/* setattr() and revattr() walk along the line from the cursor; the cursor
 * itself does not move. */
void setattr(int attr, int count)
{
    int r, c;
    _cxl_getcur(&r, &c);
    while (count-- > 0 && r < _vinfo.numrows) {
        unsigned v = _cxl_vget(r, c);
        _cxl_vput(r, c, CELL(v, attr));
        if (++c >= _vinfo.numcols) {
            c = 0;
            r++;
        }
    }
}

void revattr(int count)
{
    int r, c;
    _cxl_getcur(&r, &c);
    while (count-- > 0 && r < _vinfo.numrows) {
        unsigned v = _cxl_vget(r, c);
        unsigned char m = _vinfo.mapattr;
        _vinfo.mapattr = 0;
        _cxl_vput(r, c, CELL(v, revsattr(v >> 8)));
        _vinfo.mapattr = m;
        if (++c >= _vinfo.numcols) {
            c = 0;
            r++;
        }
    }
}

/* Cursor shape. */
static unsigned saved_shape = 0x0607;

void getcursz(int *sline, int *eline)
{
    union REGS r;
    r.h.ah = 3;
    r.h.bh = 0;
    int86(0x10, &r, &r);
    *sline = r.h.ch;
    *eline = r.h.cl;
}

void setcursz(int sline, int eline)
{
    union REGS r;
    r.h.ah = 1;
    r.h.ch = sline;
    r.h.cl = eline;
    int86(0x10, &r, &r);
}

void hidecur(void)
{
    int s, e;
    getcursz(&s, &e);
    if (!(s & 0x20))
        saved_shape = (s << 8) | e;
    setcursz(0x20, 0);
}

void showcur(void)
{
    setcursz(saved_shape >> 8, saved_shape & 0xff);
}

static int cell_height(void)
{
    return _vinfo.cheight ? _vinfo.cheight : (_vinfo.mono ? 14 : 8);
}

void smcursor(void)
{
    int h = cell_height();
    setcursz(h - 2, h - 1);
}

void lgcursor(void)
{
    setcursz(0, cell_height() - 1);
}

/* Video modes and adapters. */
int vidmode(void)
{
    union REGS r;
    r.h.ah = 0x0f;
    int86(0x10, &r, &r);
    return r.h.al;
}

void mode(int mode_code)
{
    union REGS r;
    r.h.ah = 0;
    r.h.al = mode_code;
    int86(0x10, &r, &r);
}

static int is_hercules(void)
{
    /* Bit 7 of the CRT status port toggles with vertical sync on a
     * Hercules card and stays put on an MDA. */
    unsigned char a = inportb(0x3ba) & 0x80;
    long i;
    for (i = 0; i < 30000; i++)
        if ((inportb(0x3ba) & 0x80) != a)
            return 1;
    return 0;
}

int vidtype(void)
{
    union REGS r;

    /* VGA and MCGA: read display combination code. */
    r.x.ax = 0x1a00;
    int86(0x10, &r, &r);
    if (r.h.al == 0x1a) {
        switch (r.h.bl) {
        case 7: return V_VGAMONO;
        case 8: return V_VGA;
        case 11: return V_MCGAMONO;
        case 12: return V_MCGA;
        case 4: case 5: break; /* EGA, handled below */
        case 1: goto mono;
        case 2: return V_CGA;
        }
    }
    /* EGA: get EGA information. */
    r.h.ah = 0x12;
    r.h.bl = 0x10;
    int86(0x10, &r, &r);
    if (r.h.bl != 0x10)
        return r.h.bh ? V_EGAMONO : V_EGA;
    /* CGA or mono, from the BIOS equipment word. */
    r.h.ah = 0;
    int86(0x11, &r, &r);
    if ((r.x.ax & 0x30) != 0x30)
        return V_CGA;
mono:
    if (is_hercules()) {
        unsigned char id = inportb(0x3ba) & 0x70;
        if (id == 0x50)
            return V_INCOLOR;
        if (id == 0x10)
            return V_HGCPLUS;
        return V_HGC;
    }
    return V_MDA;
}

int videoinit(void)
{
    union REGS r;
    struct SREGS s;
    int m;

    _vinfo.adapter = vidtype();
    m = vidmode();
    _vinfo.mono = (m == 7) || _vinfo.adapter == V_MDA ||
                  (_vinfo.adapter >= V_EGAMONO && _vinfo.adapter <= V_HGCPLUS);
    _vinfo.videoseg = (m == 7) ? 0xb000 : 0xb800;
    _vinfo.mapattr = _vinfo.mono;
    _vinfo.cgasnow = 0;
    _vinfo.usebios = 0;
    _vinfo.numcols = peekb(0x40, 0x4a);
    if (!_vinfo.numcols)
        _vinfo.numcols = 80;
    _vinfo.numrows = 25;
    _vinfo.cwidth = 8;
    _vinfo.cheight = _vinfo.mono ? 14 : 8;
    if (_vinfo.adapter == V_EGA || _vinfo.adapter == V_EGAMONO ||
        _vinfo.adapter >= V_MCGA || _vinfo.adapter == V_VGAMONO ||
        _vinfo.adapter == V_MCGAMONO) {
        _vinfo.numrows = (unsigned char)(peekb(0x40, 0x84) + 1);
        _vinfo.cheight = peekb(0x40, 0x85);
        if (_vinfo.numrows < 25)
            _vinfo.numrows = 25;
    }
    /* DESQview gives a program its own shadow buffer to write to. */
    _vinfo.dvexist = 0;
    if (_vinfo.dvcheck) {
        r.x.cx = 0x4445; /* 'DE' */
        r.x.dx = 0x5351; /* 'SQ' */
        r.x.ax = 0x2b01;
        intdos(&r, &r);
        if (r.h.al != 0xff) {
            _vinfo.dvexist = 1;
            segread(&s);
            s.es = _vinfo.videoseg;
            r.x.di = 0;
            r.h.ah = 0xfe;
            int86x(0x10, &r, &r, &s);
            _vinfo.videoseg = s.es;
        }
    }
    return _vinfo.adapter;
}

int setvparam(int setting)
{
    switch (setting) {
    case VP_DMA:
        _vinfo.usebios = 0;
        _vinfo.cgasnow = 0;
        break;
    case VP_CGA:
        _vinfo.usebios = 0;
        _vinfo.cgasnow = 1;
        break;
    case VP_BIOS:
        _vinfo.usebios = 1;
        _vinfo.cgasnow = 0;
        break;
    case VP_MONO:
        _vinfo.mapattr = 1;
        break;
    case VP_COLOR:
        _vinfo.mapattr = 0;
        break;
    default:
        return 1;
    }
    return 0;
}

int setlines(int numlines)
{
    union REGS r;
    int a = _vinfo.adapter;
    int ega = (a == V_EGA || a == V_EGAMONO);
    int vga = (a == V_VGA || a == V_VGAMONO);

    if (numlines == 25) {
        mode(vidmode());
    } else if (numlines == 43 && (ega || vga)) {
        if (vga) {
            r.x.ax = 0x1201; /* 350 scan lines */
            r.h.bl = 0x30;
            int86(0x10, &r, &r);
            mode(vidmode());
        }
        r.x.ax = 0x1112; /* 8x8 font */
        r.h.bl = 0;
        int86(0x10, &r, &r);
    } else if (numlines == 50 && vga) {
        r.x.ax = 0x1202; /* 400 scan lines */
        r.h.bl = 0x30;
        int86(0x10, &r, &r);
        mode(vidmode());
        r.x.ax = 0x1112;
        r.h.bl = 0;
        int86(0x10, &r, &r);
    } else {
        return 1;
    }
    _vinfo.numrows = (unsigned char)(peekb(0x40, 0x84) + 1);
    if (numlines == 25)
        _vinfo.numrows = 25;
    _vinfo.cheight = (numlines == 25) ? peekb(0x40, 0x85) : 8;
    return _vinfo.numrows != numlines;
}

char *attr2text(int attr)
{
    static const char *const names[16] = {
        "Black", "Blue", "Green", "Cyan", "Red", "Magenta", "Brown",
        "LightGrey", "DarkGrey", "LightBlue", "LightGreen", "LightCyan",
        "LightRed", "LightMagenta", "Yellow", "White"
    };
    static char buf[40];
    snprintf(buf, sizeof buf, "%s%s on %s", (attr & 0x80) ? "Blinking " : "",
             names[attr & 15], names[(attr >> 4) & 7]);
    return buf;
}

/* Whole screen save and restore. */
int *ssave(void)
{
    return _cxl_save(0, 0, _vinfo.numrows - 1, _vinfo.numcols - 1);
}

void srestore(int *sbuf)
{
    _cxl_restore(sbuf);
    free(sbuf);
}

/* CGA snow-free primitives. */
int cgardbyte(char __far *src)
{
    retrace();
    return (unsigned char)*src;
}

int cgardword(int __far *src)
{
    retrace();
    return *src;
}

void cgareadn(int __far *src, int *dest, unsigned n)
{
    while (n--) {
        retrace();
        *dest++ = *src++;
    }
}

void cgawrbyte(char __far *dest, int chr)
{
    retrace();
    *dest = chr;
}

void cgawrword(int __far *dest, int chratr)
{
    retrace();
    *dest = chratr;
}

void cgawriten(int *src, int __far *dest, unsigned n)
{
    while (n--) {
        retrace();
        *dest++ = *src++;
    }
}

void cgawrstr(char __far *dest, char *string, int attr)
{
    while (*string) {
        retrace();
        *dest++ = *string++;
        *dest++ = attr;
    }
}
