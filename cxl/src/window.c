/* CXL compatible library: window management.
 *
 * Shown windows form a stack, linked through prev (the window below) and
 * next (the window above); _winfo.active is the top. Each shown window
 * keeps a copy of the screen under it (wbuf) and under its shadow (wsbuf).
 * To work on a covered window, the windows above it are lifted off the
 * screen, top first, and put back afterwards. Hidden windows keep their
 * contents in _cbuf and are linked from _winfo.hidden through prev.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <dos.h>
#include <stdio.h>
#include "cxl-local.h"

struct _winfo_t _winfo = {
    NULL, NULL, NULL, NULL, NULL, 0, 500, 0, 0, 0, 0, 0, 1, 8, ' '
};

static void (*fillfn)(unsigned, unsigned, int, int);

int _cxl_werr(int code)
{
    _winfo.errno = code;
    return code;
}

int _cxl_wbeg(void)
{
    if (!_winfo.active)
        return _cxl_werr(W_NOACTIVE);
    return 0;
}

int _cxl_wrow0(struct _wrec_t *w) { return w->srow + w->border; }
int _cxl_wcol0(struct _wrec_t *w) { return w->scol + w->border; }
int _cxl_wrows(struct _wrec_t *w) { return w->erow - w->srow + 1 - 2 * w->border; }
int _cxl_wcols(struct _wrec_t *w) { return w->ecol - w->scol + 1 - 2 * w->border; }

void _cxl_wsyncur(void)
{
    struct _wrec_t *w = _winfo.active;
    if (w)
        _cxl_setcur(_cxl_wrow0(w) + w->row, _cxl_wcol0(w) + w->column);
}

/* Shadow: two columns down the right side and one row along the bottom,
 * keeping the characters underneath but changing their attribute. */
static int in_window(struct _wrec_t *w, int r, int c)
{
    return r >= w->srow && r <= w->erow && c >= w->scol && c <= w->ecol;
}

static int in_shadow(struct _wrec_t *w, int r, int c)
{
    if (r >= w->srow + 1 && r <= w->erow + 1 && c >= w->ecol + 1 && c <= w->ecol + 2)
        return 1;
    return r == w->erow + 1 && c >= w->scol + 2 && c <= w->ecol;
}

static void shadow_draw(struct _wrec_t *w)
{
    int r, c;
    w->wsbuf = _cxl_save(w->srow + 1, w->scol + 2, w->erow + 1, w->ecol + 2);
    for (r = w->srow + 1; r <= w->erow + 1 && r < _vinfo.numrows; r++)
        for (c = w->scol + 2; c <= w->ecol + 2 && c < _vinfo.numcols; c++)
            if (in_shadow(w, r, c)) {
                unsigned v = _cxl_vget(r, c);
                _cxl_vput(r, c, CELL(v, w->wsattr));
            }
}

static void shadow_erase(struct _wrec_t *w)
{
    int *b = w->wsbuf;
    int r, c, cols;
    if (!b)
        return;
    cols = b[3] - b[1] + 1;
    for (r = b[0]; r <= b[2]; r++)
        for (c = b[1]; c <= b[3]; c++)
            if (!in_window(w, r, c)) {
                unsigned v = (unsigned)b[4 + (r - b[0]) * cols + (c - b[1])];
                _cxl_vputraw(r, c, &v, 1);
            }
    free(b);
    w->wsbuf = NULL;
}

void _cxl_wlift(struct _wrec_t *w)
{
    free(w->_cbuf);
    w->_cbuf = _cxl_save(w->srow, w->scol, w->erow, w->ecol);
    shadow_erase(w);
    _cxl_restore(w->wbuf);
    free(w->wbuf);
    w->wbuf = NULL;
}

void _cxl_wplace(struct _wrec_t *w)
{
    w->wbuf = _cxl_save(w->srow, w->scol, w->erow, w->ecol);
    if (w->_shadow)
        shadow_draw(w);
    if (w->_cbuf) {
        _cxl_restore_at(w->_cbuf, w->srow, w->scol);
        free(w->_cbuf);
        w->_cbuf = NULL;
    }
}

/* Lifts the windows above w, top first, and returns the lowest of them
 * (still linked) for lower_back(). */
static struct _wrec_t *lift_above(struct _wrec_t *w)
{
    struct _wrec_t *t;
    if (!w->next)
        return NULL;
    for (t = _winfo.active; t != w; t = t->prev)
        _cxl_wlift(t);
    return w->next;
}

static void lower_back(struct _wrec_t *t)
{
    for (; t; t = t->next)
        _cxl_wplace(t);
}

void _cxl_wfree(struct _wrec_t *w)
{
    struct _form_t *f = w->form;
    while (f) {
        struct _form_t *p = f->prev;
        struct _field_t *d = f->field;
        while (d) {
            struct _field_t *n = d->next;
            free(d->buf);
            free(d->_slots);
            free(d->_orig);
            free(d);
            d = n;
        }
        free(f);
        f = p;
    }
    free(w->wbuf);
    free(w->wsbuf);
    free(w->_cbuf);
    free(w->title);
    free(w);
}

struct _wrec_t *_cxl_wfind(WINDOW h, int *hidden)
{
    struct _wrec_t *w;
    for (w = _winfo.active; w; w = w->prev)
        if (w->whandle == h) {
            if (hidden)
                *hidden = 0;
            return w;
        }
    for (w = _winfo.hidden; w; w = w->prev)
        if (w->whandle == h) {
            if (hidden)
                *hidden = 1;
            return w;
        }
    return NULL;
}

struct _wrec_t *wfindrec(WINDOW whandle)
{
    return _cxl_wfind(whandle, NULL);
}

WINDOW whandle(void)
{
    if (_cxl_wbeg())
        return 0;
    _cxl_werr(W_NOERROR);
    return _winfo.active->whandle;
}

/* Border and title. */
void _cxl_wdrawframe(struct _wrec_t *w)
{
    int len, width, col;
    if (!w->border)
        return;
    _cxl_box(w->srow, w->scol, w->erow, w->ecol, w->btype, w->battr);
    if (!w->title || !*w->title)
        return;
    width = w->ecol - w->scol - 1;
    len = strlen(w->title);
    if (len > width)
        len = width;
    switch (w->tpos) {
    case TRIGHT:
        col = w->ecol - len;
        break;
    case TCENTER:
        col = w->scol + 1 + (width - len) / 2;
        break;
    default:
        col = w->scol + 1;
    }
    {
        int i;
        for (i = 0; i < len; i++)
            _cxl_vput(w->srow, col + i, CELL(w->title[i], w->tattr));
    }
}

static void fill_inside(struct _wrec_t *w)
{
    int r0 = _cxl_wrow0(w), c0 = _cxl_wcol0(w);
    int r1 = r0 + _cxl_wrows(w) - 1, c1 = c0 + _cxl_wcols(w) - 1;
    if (r1 < r0 || c1 < c0)
        return;
    if (fillfn)
        fillfn((r0 << 8) | c0, (r1 << 8) | c1, _winfo.fillch, w->wattr);
    else
        _cxl_vfill(r0, c0, r1, c1, CELL(_winfo.fillch, w->wattr));
}

int wsetfill(void (*fill)(unsigned, unsigned, int, int))
{
    fillfn = fill;
    return _cxl_werr(W_NOERROR);
}

static int coords_ok(int srow, int scol, int erow, int ecol, int border)
{
    if (srow < 0 || scol < 0 || erow >= _vinfo.numrows || ecol >= _vinfo.numcols)
        return 0;
    if (erow - srow < 2 * border || ecol - scol < 2 * border)
        return 0;
    return 1;
}

WINDOW wopen(int srow, int scol, int erow, int ecol, int btype, int battr,
             int wattr)
{
    struct _wrec_t *w;
    if (btype < 0 || btype > 5) {
        _cxl_werr(W_INVBTYPE);
        return 0;
    }
    if (!coords_ok(srow, scol, erow, ecol, btype != 5)) {
        _cxl_werr(W_INVCOORD);
        return 0;
    }
    w = calloc(1, sizeof *w);
    if (!w) {
        _cxl_werr(W_ALLOCERR);
        return 0;
    }
    w->wbuf = _cxl_save(srow, scol, erow, ecol);
    if (!w->wbuf) {
        free(w);
        _cxl_werr(W_ALLOCERR);
        return 0;
    }
    w->srow = srow;
    w->scol = scol;
    w->erow = erow;
    w->ecol = ecol;
    w->btype = btype;
    w->border = btype != 5;
    w->battr = battr;
    w->wattr = wattr;
    w->attr = wattr;
    w->tpos = TLEFT;
    w->tattr = battr;
    w->help = 0;
    w->whandle = ++_winfo.handle;
    w->prev = _winfo.active;
    if (w->prev)
        w->prev->next = w;
    _winfo.active = w;
    _winfo.total++;
    if (w->border)
        _cxl_box(srow, scol, erow, ecol, btype, battr);
    fill_inside(w);
    _cxl_wsyncur();
    _cxl_werr(W_NOERROR);
    return w->whandle;
}

static void unlink_shown(struct _wrec_t *w)
{
    if (w->prev)
        w->prev->next = w->next;
    if (w->next)
        w->next->prev = w->prev;
    if (_winfo.active == w)
        _winfo.active = w->prev;
    w->prev = w->next = NULL;
}

static void unlink_hidden(struct _wrec_t *w)
{
    struct _wrec_t **pp = &_winfo.hidden;
    while (*pp && *pp != w)
        pp = &(*pp)->prev;
    if (*pp)
        *pp = w->prev;
    w->prev = w->next = NULL;
}

static void push_top(struct _wrec_t *w)
{
    w->next = NULL;
    w->prev = _winfo.active;
    if (w->prev)
        w->prev->next = w;
    _winfo.active = w;
}

int wclose(void)
{
    struct _wrec_t *w = _winfo.active;
    if (_cxl_wbeg())
        return _winfo.errno;
    _cxl_wlift(w);
    unlink_shown(w);
    _cxl_wfree(w);
    _winfo.total--;
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wcloseall(void)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    while (_winfo.active)
        wclose();
    return _cxl_werr(W_NOERROR);
}

int wunlink(WINDOW h)
{
    int hid;
    struct _wrec_t *w = _cxl_wfind(h, &hid);
    if (!w)
        return _cxl_werr(W_NOTFOUND);
    if (hid)
        unlink_hidden(w);
    else
        unlink_shown(w);
    _cxl_wfree(w);
    _winfo.total--;
    return _cxl_werr(W_NOERROR);
}

int wactiv(WINDOW h)
{
    int hid;
    struct _wrec_t *w, *above;
    if (_cxl_wbeg())
        return _winfo.errno;
    w = _cxl_wfind(h, &hid);
    if (!w || hid)
        return _cxl_werr(W_NOTFOUND);
    if (w != _winfo.active) {
        above = lift_above(w);
        _cxl_wlift(w);
        lower_back(above);
        unlink_shown(w);
        push_top(w);
        _cxl_wplace(w);
    }
    if (w->help)
        _winfo.help = w->help;
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int whide(void)
{
    struct _wrec_t *w = _winfo.active;
    if (_cxl_wbeg())
        return _winfo.errno;
    _cxl_wlift(w);
    if (!w->_cbuf)
        return _cxl_werr(W_ALLOCERR);
    unlink_shown(w);
    w->_hidden = 1;
    w->prev = _winfo.hidden;
    _winfo.hidden = w;
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wunhide(WINDOW h)
{
    int hid;
    struct _wrec_t *w;
    if (!_winfo.hidden)
        return _cxl_werr(W_NOHIDDEN);
    if (h == 0) {
        w = _winfo.hidden;
    } else {
        w = _cxl_wfind(h, &hid);
        if (!w)
            return _cxl_werr(W_NOTFOUND);
        if (!hid)
            return _cxl_werr(W_NOTHIDD);
    }
    unlink_hidden(w);
    w->_hidden = 0;
    push_top(w);
    _cxl_wplace(w);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

/* Runs fn on a shown window that may be covered. */
int _cxl_wwith(struct _wrec_t *w, int (*fn)(void *), void *arg)
{
    struct _wrec_t *saved = _winfo.active, *above;
    int r;
    above = lift_above(w);
    _winfo.active = w;
    r = fn(arg);
    _winfo.active = saved;
    lower_back(above);
    return r;
}

WINDOW windowat(int row, int col)
{
    struct _wrec_t *w;
    if (_cxl_wbeg())
        return 0;
    for (w = _winfo.active; w; w = w->prev)
        if (in_window(w, row, col)) {
            _cxl_werr(W_NOERROR);
            return w->whandle;
        }
    _cxl_werr(W_NOTFOUND);
    return 0;
}

int wshadow(int attr)
{
    struct _wrec_t *w = _winfo.active;
    if (_cxl_wbeg())
        return _winfo.errno;
    shadow_erase(w);
    w->wsattr = attr;
    w->_shadow = 1;
    shadow_draw(w);
    if (!w->wsbuf) {
        w->_shadow = 0;
        return _cxl_werr(W_ALLOCERR);
    }
    return _cxl_werr(W_NOERROR);
}

int wshadoff(void)
{
    struct _wrec_t *w = _winfo.active;
    if (_cxl_wbeg())
        return _winfo.errno;
    shadow_erase(w);
    w->_shadow = 0;
    return _cxl_werr(W_NOERROR);
}

int wtitle(char *str, int tpos, int tattr)
{
    struct _wrec_t *w = _winfo.active;
    if (_cxl_wbeg())
        return _winfo.errno;
    free(w->title);
    w->title = NULL;
    if (str) {
        w->title = malloc(strlen(str) + 1);
        if (!w->title)
            return _cxl_werr(W_ALLOCERR);
        strcpy(w->title, str);
        w->tpos = tpos;
        w->tattr = tattr;
    }
    _cxl_wdrawframe(w);
    return _cxl_werr(W_NOERROR);
}

static void clamp_cursor(struct _wrec_t *w)
{
    int rows = _cxl_wrows(w), cols = _cxl_wcols(w);
    if (w->row >= rows)
        w->row = rows > 0 ? rows - 1 : 0;
    if (w->column >= cols)
        w->column = cols > 0 ? cols - 1 : 0;
}

int wborder(int btype)
{
    struct _wrec_t *w = _winfo.active;
    int had;
    if (_cxl_wbeg())
        return _winfo.errno;
    if (btype < 0 || btype > 5)
        return _cxl_werr(W_INVBTYPE);
    had = w->border;
    if (btype != 5 && (w->erow - w->srow < 2 || w->ecol - w->scol < 2))
        return _cxl_werr(W_INVCOORD);
    w->btype = btype;
    w->border = btype != 5;
    if (had && !w->border) {
        /* The old border becomes part of the window. */
        int r, c;
        for (c = w->scol; c <= w->ecol; c++) {
            _cxl_vput(w->srow, c, CELL(' ', w->wattr));
            _cxl_vput(w->erow, c, CELL(' ', w->wattr));
        }
        for (r = w->srow; r <= w->erow; r++) {
            _cxl_vput(r, w->scol, CELL(' ', w->wattr));
            _cxl_vput(r, w->ecol, CELL(' ', w->wattr));
        }
    }
    clamp_cursor(w);
    _cxl_wdrawframe(w);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wchgattr(int battr, int wattr)
{
    struct _wrec_t *w = _winfo.active;
    int r, c, r0, c0, r1, c1;
    if (_cxl_wbeg())
        return _winfo.errno;
    r0 = _cxl_wrow0(w);
    c0 = _cxl_wcol0(w);
    r1 = r0 + _cxl_wrows(w) - 1;
    c1 = c0 + _cxl_wcols(w) - 1;
    for (r = r0; r <= r1; r++)
        for (c = c0; c <= c1; c++)
            _cxl_vput(r, c, CELL(_cxl_vget(r, c), wattr));
    if (w->tattr == w->battr)
        w->tattr = battr;
    w->battr = battr;
    w->wattr = wattr;
    w->attr = wattr;
    _cxl_wdrawframe(w);
    return _cxl_werr(W_NOERROR);
}

static int move_to(struct _wrec_t *w, int nsrow, int nscol)
{
    int h = w->erow - w->srow, wd = w->ecol - w->scol;
    if (nsrow < 0 || nscol < 0 || nsrow + h >= _vinfo.numrows ||
        nscol + wd >= _vinfo.numcols)
        return _cxl_werr(W_INVCOORD);
    _cxl_wlift(w);
    if (!w->_cbuf)
        return _cxl_werr(W_ALLOCERR);
    w->srow = nsrow;
    w->scol = nscol;
    w->erow = nsrow + h;
    w->ecol = nscol + wd;
    _cxl_wplace(w);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

int wmove(int nsrow, int nscol)
{
    if (_cxl_wbeg())
        return _winfo.errno;
    return move_to(_winfo.active, nsrow, nscol);
}

int wdrag(int direction)
{
    struct _wrec_t *w = _winfo.active;
    int r, c;
    if (_cxl_wbeg())
        return _winfo.errno;
    r = w->srow;
    c = w->scol;
    switch (direction) {
    case D_UP: r--; break;
    case D_DOWN: r++; break;
    case D_LEFT: c--; break;
    case D_RIGHT: c++; break;
    default: return _cxl_werr(W_INVCOORD);
    }
    return move_to(w, r, c);
}

int wslide(int nsrow, int nscol)
{
    struct _wrec_t *w = _winfo.active;
    int h, wd;
    if (_cxl_wbeg())
        return _winfo.errno;
    h = w->erow - w->srow;
    wd = w->ecol - w->scol;
    if (nsrow < 0 || nscol < 0 || nsrow + h >= _vinfo.numrows ||
        nscol + wd >= _vinfo.numcols)
        return _cxl_werr(W_INVCOORD);
    while (w->srow != nsrow || w->scol != nscol) {
        int r = w->srow, c = w->scol, e;
        if (r != nsrow)
            r += (r < nsrow) ? 1 : -1;
        if (c != nscol)
            c += (c < nscol) ? 1 : -1;
        e = move_to(w, r, c);
        if (e)
            return e;
        delay(8);
    }
    return _cxl_werr(W_NOERROR);
}

int wsize(int nerow, int necol)
{
    struct _wrec_t *w = _winfo.active;
    int *nb, *ob, orows, ocols, nrows, ncols, r, c, b;
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!coords_ok(w->srow, w->scol, nerow, necol, w->border))
        return _cxl_werr(W_INVCOORD);
    _cxl_wlift(w);
    ob = w->_cbuf;
    if (!ob)
        return _cxl_werr(W_ALLOCERR);
    nrows = nerow - w->srow + 1;
    ncols = necol - w->scol + 1;
    nb = malloc((4 + (size_t)nrows * ncols) * sizeof(int));
    if (!nb) {
        _cxl_wplace(w);
        return _cxl_werr(W_ALLOCERR);
    }
    nb[0] = w->srow;
    nb[1] = w->scol;
    nb[2] = nerow;
    nb[3] = necol;
    orows = ob[2] - ob[0] + 1;
    ocols = ob[3] - ob[1] + 1;
    b = w->border;
    for (r = 0; r < nrows; r++)
        for (c = 0; c < ncols; c++) {
            unsigned v = CELL(_winfo.fillch, _cxl_attr(w->wattr));
            if (r >= b && c >= b && r < orows - b && c < ocols - b &&
                r < nrows - b && c < ncols - b)
                v = (unsigned)ob[4 + r * ocols + c];
            nb[4 + r * ncols + c] = v;
        }
    free(ob);
    w->_cbuf = nb;
    w->erow = nerow;
    w->ecol = necol;
    _cxl_wplace(w);
    _cxl_wdrawframe(w);
    clamp_cursor(w);
    _cxl_wsyncur();
    return _cxl_werr(W_NOERROR);
}

WINDOW wcopy(int nsrow, int nscol)
{
    struct _wrec_t *o = _winfo.active, *n;
    int *buf;
    WINDOW h;
    if (_cxl_wbeg())
        return 0;
    buf = _cxl_save(o->srow, o->scol, o->erow, o->ecol);
    if (!buf) {
        _cxl_werr(W_ALLOCERR);
        return 0;
    }
    h = wopen(nsrow, nscol, nsrow + o->erow - o->srow, nscol + o->ecol - o->scol,
              o->btype, o->battr, o->wattr);
    if (!h) {
        free(buf);
        return 0;
    }
    n = _winfo.active;
    _cxl_restore_at(buf, nsrow, nscol);
    free(buf);
    n->attr = o->attr;
    n->row = o->row;
    n->column = o->column;
    n->help = o->help;
    if (o->title) {
        n->title = malloc(strlen(o->title) + 1);
        if (n->title)
            strcpy(n->title, o->title);
        n->tpos = o->tpos;
        n->tattr = o->tattr;
    }
    _cxl_wsyncur();
    _cxl_werr(W_NOERROR);
    return h;
}

/* Screen region save and restore. */
int *wsave(int srow, int scol, int erow, int ecol)
{
    return _cxl_save(srow, scol, erow, ecol);
}

void wrestore(int *wbuf)
{
    _cxl_restore(wbuf);
    free(wbuf);
}

char *werrmsg(void)
{
    static const char *const msg[] = {
        "No error",
        "Escape key pressed",
        "Memory allocation error",
        "Record not found",
        "No active window",
        "Invalid coordinates",
        "Invalid format string",
        "No input fields defined",
        "String too long for window",
        "Invalid box type",
        "Window has no border",
        "No hidden windows",
        "Window is not hidden",
        "No selectable menu items",
        "No menu items defined",
        "No end of menu specified",
        "No menu defined",
        "No begin of menu specified",
        "No form defined",
        "No begin of form specified",
        "No help defined",
        "Help stack overflow",
        "Help stack underflow",
        "DOS error",
        "No files matched",
        "Invalid tag id",
        "Invalid path",
    };
    int e = _winfo.errno;
    if (e < 0 || e >= (int)(sizeof msg / sizeof msg[0]))
        return "Unknown error";
    return (char *)msg[e];
}

int wperror(char *message)
{
    int len = strlen(message), r, c, sr, sc, er, ec;
    _cxl_getcur(&r, &c);
    if (len + 4 > _vinfo.numcols)
        return _cxl_werr(W_STRLONG);
    sr = r + 2;
    if (sr + 2 >= _vinfo.numrows)
        sr = r - 4 < 0 ? 0 : r - 4;
    sc = c - (len + 4) / 2;
    if (sc < 0)
        sc = 0;
    if (sc + len + 3 >= _vinfo.numcols)
        sc = _vinfo.numcols - len - 4;
    er = sr + 2;
    ec = sc + len + 3;
    if (!wopen(sr, sc, er, ec, 0, LRED | _RED, YELLOW | _RED))
        return _winfo.errno;
    wprints(0, 1, YELLOW | _RED, message);
    beep();
    clearkeys();
    getxch();
    wclose();
    return _cxl_werr(W_NOERROR);
}
