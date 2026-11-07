/* CXL compatible library: window output.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdarg.h>
#include <stdio.h>
#include "cxl-local.h"

#define W (_winfo.active)

static int inside(int wrow, int wcol)
{
    return wrow >= 0 && wcol >= 0 && wrow < _cxl_wrows(W) && wcol < _cxl_wcols(W);
}

static void put(int wrow, int wcol, int ch, int attr)
{
    _cxl_vput(_cxl_wrow0(W) + wrow, _cxl_wcol0(W) + wcol, CELL(ch, attr));
}

/* Scrolls the screen rectangle by count lines (D_UP or D_DOWN), filling
 * the new lines with blanks in attr. */
static void scroll_rect(int r0, int c0, int r1, int c1, int count, int direc,
                        int attr)
{
    int h = r1 - r0 + 1, n = c1 - c0 + 1, r;
    unsigned line[132];
    if (n <= 0 || h <= 0)
        return;
    if (n > 132)
        n = 132;
    if (count > h)
        count = h;
    if (direc == D_UP) {
        for (r = r0; r + count <= r1; r++) {
            _cxl_vgetn(r + count, c0, line, n);
            _cxl_vputraw(r, c0, line, n);
        }
        _cxl_vfill(r1 - count + 1, c0, r1, c1, CELL(_winfo.fillch, attr));
    } else {
        for (r = r1; r - count >= r0; r--) {
            _cxl_vgetn(r - count, c0, line, n);
            _cxl_vputraw(r, c0, line, n);
        }
        _cxl_vfill(r0, c0, r0 + count - 1, c1, CELL(_winfo.fillch, attr));
    }
}

static void newline(void)
{
    W->column = 0;
    if (W->row + 1 >= _cxl_wrows(W)) {
        int r0 = _cxl_wrow0(W), c0 = _cxl_wcol0(W);
        scroll_rect(r0, c0, r0 + _cxl_wrows(W) - 1, c0 + _cxl_wcols(W) - 1, 1,
                    D_UP, W->wattr);
    } else {
        W->row++;
    }
}

void _cxl_wtty(int ch)
{
    int cols = _cxl_wcols(W);
    switch (ch) {
    case '\n':
        newline();
        return;
    case '\r':
        W->column = 0;
        return;
    case '\t': {
        int tw = _winfo.tabwidth ? _winfo.tabwidth : 1;
        int to = (W->column / tw + 1) * tw;
        while (W->column < to && W->column < cols) {
            put(W->row, W->column, ' ', W->attr);
            W->column++;
        }
        if (W->column >= cols)
            newline();
        return;
    }
    case '\b':
        if (W->column > 0)
            W->column--;
        else if (W->row > 0) {
            W->row--;
            W->column = cols - 1;
        }
        return;
    case '\a':
        beep();
        return;
    }
    put(W->row, W->column, ch, W->attr);
    if (++W->column >= cols)
        newline();
}

static int word_len(const char *s)
{
    int n = 0;
    while (s[n] && (unsigned char)s[n] > ' ')
        n++;
    return n;
}

void _cxl_wttys(const char *s, int wrap)
{
    int inword = 0, wrapped = 0;
    while (*s) {
        int c = (unsigned char)*s++;
        if (c == 0x1b && *s) {
            int code = *s++;
            int a = W->attr;
            switch (code) {
            case '+': W->attr = a + 1; break;
            case '-': W->attr = a - 1; break;
            case 'I': W->attr = a ^ 0x08; break;
            case 'L': W->attr = a ^ 0x80; break;
            case 'X': W->attr = revsattr(a); break;
            case 'A':
                if (*s)
                    W->attr = (unsigned char)*s++;
                break;
            case 'F':
                if (*s)
                    W->attr = (a & 0xf0) | ((unsigned char)*s++ & 0x0f);
                break;
            case 'B':
                if (*s) {
                    int b = (unsigned char)*s++;
                    b = (b >= 16) ? (b & 0x70) : ((b & 7) << 4);
                    W->attr = (a & 0x8f) | b;
                }
                break;
            case 'R':
                if (*s) {
                    int r = (unsigned char)*s++;
                    if (r < _cxl_wrows(W))
                        W->row = r;
                }
                break;
            case 'C':
                if (*s) {
                    int k = (unsigned char)*s++;
                    if (k < _cxl_wcols(W))
                        W->column = k;
                }
                break;
            case 'E':
                if (*s) {
                    int e = *s++;
                    if (e == 'W')
                        wcclear(W->attr);
                    else if (e == 'L')
                        wclreol();
                    else if (e == 'S')
                        wclreos();
                }
                break;
            case 'D':
                if (s[0] && s[1]) {
                    int ch = (unsigned char)s[0], n = (unsigned char)s[1];
                    s += 2;
                    while (n-- > 0)
                        _cxl_wtty(ch);
                }
                break;
            }
            continue;
        }
        if (c > ' ') {
            if (wrap && !inword) {
                int len = word_len(s - 1);
                if (W->column > 0 && W->column + len > _cxl_wcols(W) &&
                    len <= _cxl_wcols(W))
                    newline();
            }
            inword = 1;
        } else {
            inword = 0;
            if (wrap && c == ' ' && W->column == 0 && wrapped)
                continue; /* no leading space after a wrap */
        }
        _cxl_wtty(c);
        wrapped = wrap && c >= ' ' && W->column == 0;
    }
}

int wputc(int ch)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    _cxl_wtty(ch & 0xff);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wputs(char *str)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    _cxl_wttys(str, 0);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wputsw(char *str)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    _cxl_wttys(str, 1);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wputns(char *str, int width)
{
    int i;
    if (_cxl_wbeg())
        return _winfo.errno;
    for (i = 0; i < width; i++)
        _cxl_wtty(*str ? (unsigned char)*str++ : ' ');
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wdupc(int ch, int count)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    while (count-- > 0)
        _cxl_wtty(ch & 0xff);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wdups(char *str, int count)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    while (count-- > 0)
        _cxl_wttys(str, 0);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wprintf(const char *format, ...)
{
    static char buf[512];
    va_list ap;
    if (_cxl_wbeg())
        return _winfo.errno;
    va_start(ap, format);
    vsnprintf(buf, sizeof buf, format, ap);
    va_end(ap);
    _cxl_wttys(buf, 0);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wprintc(int wrow, int wcol, int attr, int ch)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!inside(wrow, wcol))
        return _cxl_werr(W_INVCOORD);
    put(wrow, wcol, ch, attr);
    return _cxl_werr(W_NOERROR);
}

int wprints(int wrow, int wcol, int attr, char *str)
{
    int cols;
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!inside(wrow, wcol))
        return _cxl_werr(W_INVCOORD);
    cols = _cxl_wcols(W);
    while (*str && wcol < cols)
        put(wrow, wcol++, (unsigned char)*str++, attr);
    return _cxl_werr(*str ? W_STRLONG : W_NOERROR);
}

static int wwprints_fn(void *arg)
{
    void **a = arg;
    int *v = a[0];
    return wprints(v[0], v[1], v[2], a[1]);
}

int wwprints(WINDOW whandle, int wrow, int wcol, int attr, char *str)
{
    int hid, v[3];
    void *a[2];
    struct _wrec_t *w = _cxl_wfind(whandle, &hid);
    if (!w)
        return _cxl_werr(W_NOTFOUND);
    if (hid) {
        /* Write into the saved contents. */
        int *b = w->_cbuf, cols = w->ecol - w->scol + 1;
        int r = wrow + w->border, c = wcol + w->border;
        int icols = cols - 2 * w->border;
        if (wrow < 0 || wcol < 0 || r >= w->erow - w->srow + 1 - w->border ||
            wcol >= icols)
            return _cxl_werr(W_INVCOORD);
        while (*str && c < cols - w->border)
            b[4 + r * cols + c++] = CELL(*str++, _cxl_attr(attr));
        return _cxl_werr(*str ? W_STRLONG : W_NOERROR);
    }
    v[0] = wrow;
    v[1] = wcol;
    v[2] = attr;
    a[0] = v;
    a[1] = str;
    return _cxl_wwith(w, wwprints_fn, a);
}

int wprintsf(int wrow, int wcol, int attr, char *format, char *str)
{
    struct _cxl_slot *sl;
    int n, i, cols;
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!inside(wrow, wcol))
        return _cxl_werr(W_INVCOORD);
    n = _cxl_fmtparse(format, 0, 0, &sl);
    if (n < 0)
        return _cxl_werr(W_INVFORMT);
    cols = _cxl_wcols(W);
    for (i = 0; i < n && wcol < cols; i++) {
        int ch;
        if (sl[i].kind == SL_LIT)
            ch = sl[i].ch;
        else if (sl[i].kind == SL_DECIMAL)
            ch = '.';
        else if (*str) {
            ch = (unsigned char)*str++;
            if (_cxl_slotok(&sl[i], ch, 0, 0) < 0)
                ch = '?';
        } else
            ch = ' ';
        put(wrow, wcol++, ch, attr);
    }
    free(sl);
    return _cxl_werr(*str ? W_STRLONG : W_NOERROR);
}

int wcenters(int wrow, int attr, char *str)
{
    int len = strlen(str), cols;
    if (_cxl_wbeg())
        return _winfo.errno;
    cols = _cxl_wcols(W);
    if (wrow < 0 || wrow >= _cxl_wrows(W))
        return _cxl_werr(W_INVCOORD);
    if (len > cols)
        return _cxl_werr(W_STRLONG);
    return wprints(wrow, (cols - len) / 2, attr, str);
}

int wrjusts(int wrow, int wjcol, int attr, char *str)
{
    int len = strlen(str);
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!inside(wrow, wjcol))
        return _cxl_werr(W_INVCOORD);
    if (len > wjcol + 1)
        return _cxl_werr(W_STRLONG);
    return wprints(wrow, wjcol - len + 1, attr, str);
}

int wgotoxy(int wrow, int wcol)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!inside(wrow, wcol))
        return _cxl_werr(W_INVCOORD);
    W->row = wrow;
    W->column = wcol;
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wpgotoxy(int wrow, int wcol)
{
    int rows, cols;
    if (_cxl_wbeg())
        return _winfo.errno;
    rows = _cxl_wrows(W);
    cols = _cxl_wcols(W);
    if (wcol < 0 || wrow < 0)
        return _cxl_werr(W_INVCOORD);
    wrow += wcol / cols;
    wcol %= cols;
    if (wrow >= rows)
        return _cxl_werr(W_INVCOORD);
    return wgotoxy(wrow, wcol);
}

int wreadcur(int *wrow, int *wcol)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    *wrow = W->row;
    *wcol = W->column;
    return _cxl_werr(W_NOERROR);
}

int wtextattr(int attr)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    W->attr = attr;
    return _cxl_werr(W_NOERROR);
}

static void fill_win(int r0, int c0, int r1, int c1, int ch, int attr)
{
    _cxl_vfill(_cxl_wrow0(W) + r0, _cxl_wcol0(W) + c0, _cxl_wrow0(W) + r1,
               _cxl_wcol0(W) + c1, CELL(ch, attr));
}

int wcclear(int attr)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    fill_win(0, 0, _cxl_wrows(W) - 1, _cxl_wcols(W) - 1, _winfo.fillch, attr);
    W->row = W->column = 0;
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wclreol(void)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    fill_win(W->row, W->column, W->row, _cxl_wcols(W) - 1, _winfo.fillch, W->attr);
    return _cxl_werr(W_NOERROR);
}

int wclreos(void)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    wclreol();
    if (W->row + 1 < _cxl_wrows(W))
        fill_win(W->row + 1, 0, _cxl_wrows(W) - 1, _cxl_wcols(W) - 1,
                 _winfo.fillch, W->attr);
    return _cxl_werr(W_NOERROR);
}

int wchkrow(int wrow)
{
    return !_winfo.active || wrow < 0 || wrow >= _cxl_wrows(W);
}

int wchkcol(int wcol)
{
    return !_winfo.active || wcol < 0 || wcol >= _cxl_wcols(W);
}

int wchkcoord(int wrow, int wcol)
{
    return wchkrow(wrow) || wchkcol(wcol);
}

int wchkbox(int wsrow, int wscol, int werow, int wecol)
{
    return wchkcoord(wsrow, wscol) || wchkcoord(werow, wecol) ||
           werow < wsrow || wecol < wscol;
}

int wfill(int wsrow, int wscol, int werow, int wecol, int ch, int attr)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    if (wchkbox(wsrow, wscol, werow, wecol))
        return _cxl_werr(W_INVCOORD);
    fill_win(wsrow, wscol, werow, wecol, ch, attr);
    return _cxl_werr(W_NOERROR);
}

int wbox(int wsrow, int wscol, int werow, int wecol, int btype, int attr)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    if (wchkbox(wsrow, wscol, werow, wecol))
        return _cxl_werr(W_INVCOORD);
    if (btype < 0 || btype > 5)
        return _cxl_werr(W_INVBTYPE);
    _cxl_box(_cxl_wrow0(W) + wsrow, _cxl_wcol0(W) + wscol,
             _cxl_wrow0(W) + werow, _cxl_wcol0(W) + wecol, btype, attr);
    return _cxl_werr(W_NOERROR);
}

int wbprintc(int bord, int offs, int attr, int ch)
{
    int r, c;
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!W->border)
        return _cxl_werr(W_NOBORDER);
    switch (bord) {
    case TP_BORD: r = W->srow; c = W->scol + offs; break;
    case BT_BORD: r = W->erow; c = W->scol + offs; break;
    case LT_BORD: r = W->srow + offs; c = W->scol; break;
    case RT_BORD: r = W->srow + offs; c = W->ecol; break;
    default: return _cxl_werr(W_INVCOORD);
    }
    if (offs < 0 || r > W->erow || c > W->ecol)
        return _cxl_werr(W_INVCOORD);
    _cxl_vput(r, c, CELL(ch, attr));
    return _cxl_werr(W_NOERROR);
}

int wmessage(char *str, int border, int leftofs, int attr)
{
    int r, c, len = strlen(str);
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!W->border)
        return _cxl_werr(W_NOBORDER);
    r = (border == BT_BORD) ? W->erow : W->srow;
    c = W->scol + leftofs;
    if (leftofs < 0 || c + len > W->ecol)
        return _cxl_werr(W_STRLONG);
    while (*str)
        _cxl_vput(r, c++, CELL(*str++, attr));
    return _cxl_werr(W_NOERROR);
}

/* Line drawing. Each box character is described by its four arms (up,
 * down, left, right), 0 none, 1 single, 2 double, two bits each. */
#define ARMS(u, d, l, r) ((u) | ((d) << 2) | ((l) << 4) | ((r) << 6))
static const unsigned char arms[0xdb - 0xb3] = {
    ARMS(1, 1, 0, 0), ARMS(1, 1, 1, 0), ARMS(1, 1, 2, 0), ARMS(2, 2, 1, 0), /* b3-b6 */
    ARMS(0, 2, 1, 0), ARMS(0, 1, 2, 0), ARMS(2, 2, 2, 0), ARMS(2, 2, 0, 0), /* b7-ba */
    ARMS(0, 2, 2, 0), ARMS(2, 0, 2, 0), ARMS(2, 0, 1, 0), ARMS(1, 0, 2, 0), /* bb-be */
    ARMS(0, 1, 1, 0), ARMS(1, 0, 0, 1), ARMS(1, 0, 1, 1), ARMS(0, 1, 1, 1), /* bf-c2 */
    ARMS(1, 1, 0, 1), ARMS(0, 0, 1, 1), ARMS(1, 1, 1, 1), ARMS(1, 1, 0, 2), /* c3-c6 */
    ARMS(2, 2, 0, 1), ARMS(2, 0, 0, 2), ARMS(0, 2, 0, 2), ARMS(2, 0, 2, 2), /* c7-ca */
    ARMS(0, 2, 2, 2), ARMS(2, 2, 0, 2), ARMS(0, 0, 2, 2), ARMS(2, 2, 2, 2), /* cb-ce */
    ARMS(1, 0, 2, 2), ARMS(2, 0, 1, 1), ARMS(0, 1, 2, 2), ARMS(0, 2, 1, 1), /* cf-d2 */
    ARMS(2, 0, 0, 1), ARMS(1, 0, 0, 2), ARMS(0, 1, 0, 2), ARMS(0, 2, 0, 1), /* d3-d6 */
    ARMS(2, 2, 1, 1), ARMS(1, 1, 2, 2), ARMS(1, 0, 1, 0), ARMS(0, 1, 0, 1), /* d7-da */
};

static int arms_of(int ch)
{
    return (ch >= 0xb3 && ch <= 0xda) ? arms[ch - 0xb3] : 0;
}

static int char_of(int a)
{
    int i, u, d, l, r;
    for (i = 0; i < (int)sizeof arms; i++)
        if (arms[i] == a)
            return 0xb3 + i;
    /* No such character: make the vertical and horizontal weights agree. */
    u = a & 3;
    d = (a >> 2) & 3;
    l = (a >> 4) & 3;
    r = (a >> 6) & 3;
    if (u && d)
        u = d = (u > d ? u : d);
    if (l && r)
        l = r = (l > r ? l : r);
    if ((u || d) && (l || r)) {
        int v = u ? u : d, h = l ? l : r;
        a = ARMS(u ? v : 0, d ? v : 0, l ? h : 0, r ? h : 0);
        for (i = 0; i < (int)sizeof arms; i++)
            if (arms[i] == a)
                return 0xb3 + i;
        a = ARMS(u ? 1 : 0, d ? 1 : 0, l ? 1 : 0, r ? 1 : 0);
    } else {
        a = ARMS(u, d, l, r);
    }
    for (i = 0; i < (int)sizeof arms; i++)
        if (arms[i] == a)
            return 0xb3 + i;
    return 0;
}

/* Adds arms to the screen cell, joining an existing line. */
static void merge(int row, int col, int add, int attr)
{
    unsigned v = _cxl_vget(row, col);
    int a = arms_of(v & 0xff), i, ch;
    for (i = 0; i < 8; i += 2)
        if ((add >> i) & 3)
            a = (a & ~(3 << i)) | (add & (3 << i));
    ch = char_of(a);
    if (ch)
        _cxl_vput(row, col, CELL(ch, attr));
}

static int line_weight(int btype, int horizontal)
{
    switch (btype) {
    case 0: return 1;
    case 1: return 2;
    case 2: return horizontal ? 1 : 2;
    case 3: return horizontal ? 2 : 1;
    }
    return 0;
}

int whline(int wsrow, int wscol, int count, int btype, int attr)
{
    int i, wt, r, c0;
    if (_cxl_wbeg())
        return _winfo.errno;
    if (btype < 0 || btype > 5)
        return _cxl_werr(W_INVBTYPE);
    if (count <= 0 || wchkcoord(wsrow, wscol) || wchkcol(wscol + count - 1))
        return _cxl_werr(W_INVCOORD);
    wt = line_weight(btype, 1);
    r = _cxl_wrow0(W) + wsrow;
    c0 = _cxl_wcol0(W) + wscol;
    for (i = 0; i < count; i++) {
        if (!wt) {
            _cxl_vput(r, c0 + i, CELL(btype == 4 ? 0xdc : ' ', attr));
            continue;
        }
        merge(r, c0 + i, ARMS(0, 0, i > 0 || wscol == 0 ? wt : 0,
                              i < count - 1 || wscol + count == _cxl_wcols(W) ? wt : 0),
              attr);
    }
    if (wt && W->border) {
        if (wscol == 0)
            merge(r, W->scol, ARMS(0, 0, 0, wt), W->battr);
        if (wscol + count == _cxl_wcols(W))
            merge(r, W->ecol, ARMS(0, 0, wt, 0), W->battr);
    }
    return _cxl_werr(W_NOERROR);
}

int wvline(int wsrow, int wscol, int count, int btype, int attr)
{
    int i, wt, r0, c;
    if (_cxl_wbeg())
        return _winfo.errno;
    if (btype < 0 || btype > 5)
        return _cxl_werr(W_INVBTYPE);
    if (count <= 0 || wchkcoord(wsrow, wscol) || wchkrow(wsrow + count - 1))
        return _cxl_werr(W_INVCOORD);
    wt = line_weight(btype, 0);
    r0 = _cxl_wrow0(W) + wsrow;
    c = _cxl_wcol0(W) + wscol;
    for (i = 0; i < count; i++) {
        if (!wt) {
            _cxl_vput(r0 + i, c, CELL(btype == 4 ? 0xdb : ' ', attr));
            continue;
        }
        merge(r0 + i, c, ARMS(i > 0 || wsrow == 0 ? wt : 0,
                              i < count - 1 || wsrow + count == _cxl_wrows(W) ? wt : 0,
                              0, 0),
              attr);
    }
    if (wt && W->border) {
        if (wsrow == 0)
            merge(W->srow, c, ARMS(0, wt, 0, 0), W->battr);
        if (wsrow + count == _cxl_wrows(W))
            merge(W->erow, c, ARMS(wt, 0, 0, 0), W->battr);
    }
    return _cxl_werr(W_NOERROR);
}

/* Scrolling. */
int wscrollbox(int wsrow, int wscol, int werow, int wecol, int count,
               int direction)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    if (wchkbox(wsrow, wscol, werow, wecol))
        return _cxl_werr(W_INVCOORD);
    scroll_rect(_cxl_wrow0(W) + wsrow, _cxl_wcol0(W) + wscol,
                _cxl_wrow0(W) + werow, _cxl_wcol0(W) + wecol, count,
                direction == D_DOWN ? D_DOWN : D_UP, W->attr);
    return _cxl_werr(W_NOERROR);
}

int wscroll(int count, int direc)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    return wscrollbox(0, 0, _cxl_wrows(W) - 1, _cxl_wcols(W) - 1, count, direc);
}

int wdelline(int wrow, int direc)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    if (wchkrow(wrow))
        return _cxl_werr(W_INVCOORD);
    if (direc == D_DOWN)
        return wscrollbox(0, 0, wrow, _cxl_wcols(W) - 1, 1, D_DOWN);
    return wscrollbox(wrow, 0, _cxl_wrows(W) - 1, _cxl_wcols(W) - 1, 1, D_UP);
}

int winsline(int wrow, int direc)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    if (wchkrow(wrow))
        return _cxl_werr(W_INVCOORD);
    if (direc == D_UP)
        return wscrollbox(0, 0, wrow, _cxl_wcols(W) - 1, 1, D_UP);
    return wscrollbox(wrow, 0, _cxl_wrows(W) - 1, _cxl_wcols(W) - 1, 1, D_DOWN);
}
