/* CXL compatible library: bar menus.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <ctype.h>
#include "cxl-local.h"

static struct _menu_t *defm; /* menu being defined */
static struct _item_t *defi; /* last item defined in defm */

enum { A_ESC, A_SELECT, A_ALL, A_PREV, A_NEXT, A_ERR };

static struct _item_t *last_item(struct _menu_t *m)
{
    struct _item_t *i = m ? m->item : NULL;
    while (i && i->next)
        i = i->next;
    return i;
}

static void free_menu(struct _menu_t *m)
{
    struct _item_t *i = m->item;
    while (i) {
        struct _item_t *n = i->next;
        if (i->child)
            free_menu(i->child);
        free(i);
        i = n;
    }
    free(m);
}

static int begin(int srow, int scol, int erow, int ecol, int btype, int battr,
                 int wattr, void (*open)(void), int usecurr)
{
    struct _menu_t *m;
    if (defm && !defi)
        return _cxl_werr(W_NOITMDEF);
    m = calloc(1, sizeof *m);
    if (!m)
        return _cxl_werr(W_ALLOCERR);
    m->srow = srow;
    m->scol = scol;
    m->erow = erow;
    m->ecol = ecol;
    m->btype = btype;
    m->battr = battr;
    m->wattr = wattr;
    m->open = open;
    m->usecurr = usecurr;
    if (defm) {
        m->parent = defm;
        defi->child = m;
    } else {
        m->prev = _winfo.menu;
        if (m->prev)
            m->prev->next = m;
        _winfo.menu = m;
    }
    defm = m;
    defi = NULL;
    return _cxl_werr(W_NOERROR);
}

int wmenubeg(int srow, int scol, int erow, int ecol, int btype, int battr,
             int wattr, void (*open)(void))
{
    return begin(srow, scol, erow, ecol, btype, battr, wattr, open, 0);
}

int wmenubegc(void)
{
    return begin(0, 0, 0, 0, 0, 0, 0, NULL, 1);
}

int wmenuitem(int wrow, int wcol, char *str, int schar, int tagid, int fmask,
              void (*select)(void), unsigned hotkey, int help)
{
    struct _item_t *i, *t;
    if (!defm)
        return _cxl_werr(W_NOMNUBEG);
    i = calloc(1, sizeof *i);
    if (!i)
        return _cxl_werr(W_ALLOCERR);
    i->wrow = wrow;
    i->wcol = wcol;
    i->str = str;
    i->schar = schar;
    i->tagid = tagid;
    i->fmask = fmask;
    i->select = select;
    i->hotkey = hotkey;
    i->help = help;
    t = last_item(defm);
    if (t) {
        t->next = i;
        i->prev = t;
    } else {
        defm->item = i;
    }
    defi = i;
    return _cxl_werr(W_NOERROR);
}

int wmenuiba(void (*before)(void), void (*after)(void))
{
    if (!defi)
        return _cxl_werr(W_NOITMDEF);
    defi->before = before;
    defi->after = after;
    return _cxl_werr(W_NOERROR);
}

int wmenuitxt(int wrow, int wcol, int attr, char *str)
{
    if (!defi)
        return _cxl_werr(W_NOITMDEF);
    defi->dwrow = wrow;
    defi->dwcol = wcol;
    defi->dattr = attr;
    defi->desc = str;
    return _cxl_werr(W_NOERROR);
}

static struct _item_t *find_in(struct _menu_t *m, int tag)
{
    struct _item_t *i;
    for (i = m->item; i; i = i->next)
        if (i->tagid == tag)
            return i;
    return NULL;
}

int wmenuend(int taginit, int menutype, int barwidth, int textpos,
             int textattr, int scharattr, int noselattr, int barattr)
{
    struct _menu_t *m = defm;
    if (!m)
        return _cxl_werr(W_NOMNUBEG);
    if (!m->item)
        return _cxl_werr(W_NOITMDEF);
    if (!find_in(m, taginit))
        return _cxl_werr(W_INVTAGID);
    m->_taginit = taginit;
    m->tagcurr = taginit;
    m->menutype = menutype;
    m->barwidth = barwidth;
    m->textpos = textpos;
    m->textattr = textattr;
    m->scharattr = scharattr;
    m->noselattr = noselattr;
    m->barattr = barattr;
    m->_ended = 1;
    defm = m->parent;
    defi = last_item(defm);
    return _cxl_werr(W_NOERROR);
}

/* Searches the menu tree under m for a tag, returning the item and the
 * menu that holds it. */
static struct _item_t *find_tree(struct _menu_t *m, int tag, struct _menu_t **in)
{
    struct _item_t *i;
    for (i = m->item; i; i = i->next) {
        if (i->tagid == tag) {
            if (in)
                *in = m;
            return i;
        }
        if (i->child) {
            struct _item_t *r = find_tree(i->child, tag, in);
            if (r)
                return r;
        }
    }
    return NULL;
}

static struct _item_t *find_any(int tag, struct _menu_t **in)
{
    struct _menu_t *r;
    for (r = _winfo.menu; r; r = r->prev) {
        struct _item_t *i = find_tree(r, tag, in);
        if (i)
            return i;
    }
    return NULL;
}

struct _item_t *wmenuifind(int tagid)
{
    struct _item_t *i;
    if (!_winfo.menu) {
        _cxl_werr(W_NOMNUDEF);
        return NULL;
    }
    i = find_any(tagid, NULL);
    _cxl_werr(i ? W_NOERROR : W_NOTFOUND);
    return i;
}

static int set_flags(int tagid, int set, int clr)
{
    struct _item_t *i = wmenuifind(tagid);
    if (!i)
        return _winfo.errno;
    i->fmask = (i->fmask | set) & ~clr;
    i->redisp = 1;
    return W_NOERROR;
}

int wmenuidsab(int tagid) { return set_flags(tagid, M_NOSEL, 0); }
int wmenuienab(int tagid) { return set_flags(tagid, 0, M_NOSEL); }
int wmenuiredisp(int tagid) { return set_flags(tagid, 0, 0); }

int wmenuinext(int tagid)
{
    struct _menu_t *m;
    if (!_winfo.menu)
        return _cxl_werr(W_NOMNUDEF);
    if (!find_any(tagid, &m))
        return _cxl_werr(W_NOTFOUND);
    m->tagcurr = tagid;
    return _cxl_werr(W_NOERROR);
}

char *wmenuigetxt(int tagid)
{
    struct _item_t *i = wmenuifind(tagid);
    return i ? i->str : NULL;
}

int wmenuisetxt(int tagid, char *str)
{
    struct _item_t *i = wmenuifind(tagid);
    if (!i)
        return _winfo.errno;
    i->str = str;
    i->redisp = 1;
    return W_NOERROR;
}

/* Drawing. The menu draws into its own window even when other windows
 * are on top of it. */
struct drawarg {
    struct _menu_t *m;
    struct _item_t *i;
    int bar;
};

static int draw_fn(void *p)
{
    struct drawarg *d = p;
    struct _menu_t *m = d->m;
    struct _item_t *i = d->i;
    struct _wrec_t *w = _winfo.active;
    int cols = _cxl_wcols(w), len = strlen(i->str), bw, tp, k, sc = -1;
    int nosel = i->fmask & M_NOSEL;
    int attr = d->bar ? m->barattr : nosel ? m->noselattr : m->textattr;

    if (i->wrow >= _cxl_wrows(w) || i->wcol >= cols)
        return 0;
    bw = m->barwidth ? m->barwidth : len;
    tp = m->barwidth ? m->textpos : 0;
    if (i->wcol + bw > cols)
        bw = cols - i->wcol;
    for (k = 0; k < bw; k++)
        wprintc(i->wrow, i->wcol + k, attr, ' ');
    if (i->schar && !nosel && !d->bar) {
        const char *p = strchr(i->str, i->schar);
        for (k = 0; !p && i->str[k]; k++)
            if (toupper((unsigned char)i->str[k]) == toupper(i->schar))
                p = i->str + k;
        if (p)
            sc = p - i->str;
    }
    for (k = 0; k < len && tp + k < bw; k++)
        wprintc(i->wrow, i->wcol + tp + k, k == sc ? m->scharattr : attr,
                (unsigned char)i->str[k]);
    if (d->bar && i->desc)
        wprints(i->dwrow, i->dwcol, i->dattr, i->desc);
    return 0;
}

static void draw(struct _menu_t *m, struct _item_t *i, int bar)
{
    struct drawarg d;
    struct _wrec_t *w = _cxl_wfind(m->_win, NULL);
    if (!w || w->_hidden)
        return;
    d.m = m;
    d.i = i;
    d.bar = bar;
    if (w == _winfo.active)
        draw_fn(&d);
    else
        _cxl_wwith(w, draw_fn, &d);
}

static void draw_all(struct _menu_t *m)
{
    struct _item_t *i;
    for (i = m->item; i; i = i->next) {
        draw(m, i, i == m->citem);
        i->redisp = 0;
    }
}

static int selectable(struct _item_t *i)
{
    return i && !(i->fmask & M_NOSEL);
}

static struct _item_t *step(struct _menu_t *m, struct _item_t *from, int fwd)
{
    struct _item_t *i = from;
    do {
        if (fwd)
            i = i->next ? i->next : m->item;
        else
            i = i->prev ? i->prev : last_item(m);
    } while (i != from && !selectable(i));
    return selectable(i) ? i : from;
}

/* Omnidirectional menus move to the nearest item in the key's direction. */
static struct _item_t *nearest(struct _menu_t *m, struct _item_t *c, int dir)
{
    struct _item_t *i, *best = NULL;
    long bs = 0;
    for (i = m->item; i; i = i->next) {
        long dr = i->wrow - c->wrow, dc = (long)i->wcol - c->wcol, s;
        if (i == c || !selectable(i))
            continue;
        switch (dir) {
        case D_DOWN: if (dr <= 0) continue; s = dr * 256 + (dc < 0 ? -dc : dc); break;
        case D_UP: if (dr >= 0) continue; s = -dr * 256 + (dc < 0 ? -dc : dc); break;
        case D_RIGHT: if (dc <= 0) continue; s = dc + (dr < 0 ? -dr : dr) * 256; break;
        default: if (dc >= 0) continue; s = -dc + (dr < 0 ? -dr : dr) * 256; break;
        }
        if (!best || s < bs) {
            best = i;
            bs = s;
        }
    }
    return best ? best : c;
}

static void enter_item(struct _menu_t *m)
{
    struct _item_t *i = m->citem;
    m->tagcurr = i->tagid;
    _winfo.help = i->help;
    if (i->before) {
        _winfo.cmenu = m;
        i->before();
    }
    draw(m, i, 1);
}

static void leave_item(struct _menu_t *m)
{
    struct _item_t *i = m->citem;
    draw(m, i, 0);
    if (i->after) {
        _winfo.cmenu = m;
        i->after();
    }
}

static void move_to(struct _menu_t *m, struct _item_t *to)
{
    if (!to || to == m->citem)
        return;
    leave_item(m);
    m->citem = to;
    enter_item(m);
}

/* Applies what the callbacks asked for: redrawn items and a new bar
 * position. */
static void sync(struct _menu_t *m)
{
    int n;
    struct _item_t *i;
    for (i = m->item; i; i = i->next)
        if (i->redisp) {
            i->redisp = 0;
            draw(m, i, i == m->citem);
        }
    for (n = 0; n < 4 && m->tagcurr != m->citem->tagid; n++) {
        struct _item_t *t = find_in(m, m->tagcurr);
        if (!t) {
            m->tagcurr = m->citem->tagid;
            break;
        }
        move_to(m, t);
    }
}

static void close_menu(struct _menu_t *m)
{
    if (!m->_win)
        return;
    leave_item(m);
    if (!m->usecurr && _cxl_wfind(m->_win, NULL)) {
        if (!_winfo.active || _winfo.active->whandle != m->_win)
            wactiv(m->_win);
        wclose();
    }
    m->_win = 0;
    if (!(m->menutype & M_SAVE))
        m->tagcurr = m->_taginit;
}

static struct _menu_t *root_of(struct _menu_t *m)
{
    while (m->parent)
        m = m->parent;
    return m;
}

static struct _item_t *hotkey_item(struct _menu_t *m, unsigned key)
{
    struct _item_t *i;
    for (i = m->item; i; i = i->next) {
        if (i->hotkey && i->hotkey == key && selectable(i))
            return i;
        if (i->child) {
            struct _item_t *r = hotkey_item(i->child, key);
            if (r)
                return r;
        }
    }
    return NULL;
}

static struct _item_t *item_at(struct _menu_t *m, int row, int col)
{
    struct _wrec_t *w = _cxl_wfind(m->_win, NULL);
    struct _item_t *i;
    if (!w)
        return NULL;
    row -= _cxl_wrow0(w);
    col -= _cxl_wcol0(w);
    for (i = m->item; i; i = i->next) {
        int bw = m->barwidth ? m->barwidth : (int)strlen(i->str);
        if (row == i->wrow && col >= i->wcol && col < i->wcol + bw)
            return i;
    }
    return NULL;
}

static int run(struct _menu_t *m, int *tag);

/* Selects m->citem. Returns -1 to stay in the menu, or an action. */
static int choose(struct _menu_t *m, int *tag)
{
    struct _item_t *it = m->citem;
    int act, t;

    if (!selectable(it))
        return -1;
    switch (it->tagid) {
    case M_EXIT: return A_ESC;
    case M_EXITALL: *tag = it->tagid; return A_ALL;
    case M_PREVPD: if (m->menutype & M_PD) return A_PREV; break;
    case M_NEXTPD: if (m->menutype & M_PD) return A_NEXT; break;
    }
    if (it->child) {
        act = run(it->child, &t);
        /* Left and right in a pull-down move along the bar. */
        while (act == A_PREV || act == A_NEXT) {
            move_to(m, step(m, m->citem, act == A_NEXT));
            sync(m);
            it = m->citem;
            if (!it->child) {
                act = A_ESC;
                break;
            }
            act = run(it->child, &t);
        }
        if (act == A_ERR)
            return A_ERR;
        if (act == A_ALL) {
            *tag = t;
            return A_ALL;
        }
        if (act == A_ESC)
            return -1;
    }
    if (it->select) {
        if (it->fmask & M_CLOSB) {
            close_menu(m);
            _winfo.cmenu = m;
            it->select();
            *tag = it->tagid;
            return (it->fmask & M_CLALL) ? A_ALL : A_SELECT;
        }
        _winfo.cmenu = m;
        _kbinfo.inmenu = 0;
        it->select();
        _kbinfo.inmenu = 1;
        _winfo.cmenu = m;
        if (it->fmask & M_CLALL) {
            *tag = it->tagid;
            return A_ALL;
        }
        if (it->fmask & M_CLOSE) {
            *tag = it->tagid;
            return A_SELECT;
        }
        return -1;
    }
    /* A selection in a pull-down returns the tag of the item it hangs
       from: wmenuget() reports the root menu's item. */
    *tag = it->tagid;
    return (it->fmask & M_CLALL) ? A_ALL : A_SELECT;
}

/* Handles a hot key from anywhere in the menu tree. Returns -2 if no item
 * has it, -1 to stay in the menu, or an action. */
static int hotkey(struct _menu_t *m, unsigned key, int *tag)
{
    struct _item_t *i = hotkey_item(root_of(m), key);
    if (!i)
        return -2;
    if (i->select) {
        _kbinfo.inmenu = 0;
        i->select();
        _kbinfo.inmenu = 1;
        return -1;
    }
    if (i->tagid == M_EXITALL) {
        *tag = i->tagid;
        return A_ALL;
    }
    if (find_in(m, i->tagid) == i) {
        move_to(m, i);
        return choose(m, tag);
    }
    return -1;
}

static int run(struct _menu_t *m, int *tag)
{
    struct _menu_t *savecm = _winfo.cmenu;
    struct _item_t *start;
    int horz = m->menutype & M_HORZ, vert = m->menutype & M_VERT;
    int omni = (m->menutype & M_OMNI) == M_OMNI;
    int act = A_ESC;

    if (!m->_ended)
        return _cxl_werr(W_NOMNUEND), A_ERR;
    if (m->usecurr || (m->menutype & 0x80)) { /* 0x80: TCXL MNU_USE */
        m->usecurr = 1;
        if (!_winfo.active)
            return _cxl_werr(W_NOACTIVE), A_ERR;
        m->_win = _winfo.active->whandle;
    } else {
        m->_win = wopen(m->srow, m->scol, m->erow, m->ecol, m->btype, m->battr,
                        m->wattr);
        if (!m->_win)
            return A_ERR;
    }
    _winfo.mlevel++;
    _winfo.cmenu = m;
    _kbinfo.inmenu = 1;
    start = find_in(m, (m->menutype & M_SAVE) ? m->tagcurr : m->_taginit);
    if (!start)
        start = m->item;
    if (!selectable(start))
        start = step(m, start, 1);
    m->citem = start;
    if (m->open)
        m->open();
    if (!selectable(start)) {
        draw_all(m);
        _cxl_werr(W_NOSELECT);
        close_menu(m);
        act = A_ERR;
        goto out;
    }
    m->citem = start;
    {
        struct _item_t *i;
        for (i = m->item; i; i = i->next)
            draw(m, i, 0);
    }
    enter_item(m);

    for (;;) {
        unsigned key;
        int c, r = -1;
        struct _item_t *i;

        _winfo.cmenu = m;
        sync(m);
        key = getxch();
        c = key & 0xff;

        if (c == 0x1b) {
            if (!m->parent && !_winfo.esc)
                continue;
            act = A_ESC;
            break;
        }
        if (c == '\r' && _kbinfo.source == 2 && (_mouse & MS_CURS)) {
            i = item_at(m, _cxl_mrow, _cxl_mcol);
            if (!i || !selectable(i))
                continue;
            move_to(m, i);
            r = choose(m, tag);
        } else if (c == '\r') {
            r = choose(m, tag);
        } else if (c == 0) {
            switch (key >> 8) {
            case 0x48: /* up */
                if (omni)
                    move_to(m, nearest(m, m->citem, D_UP));
                else if (vert)
                    move_to(m, step(m, m->citem, 0));
                break;
            case 0x50: /* down */
                if (omni)
                    move_to(m, nearest(m, m->citem, D_DOWN));
                else if (vert)
                    move_to(m, step(m, m->citem, 1));
                else if (m->citem->child)
                    r = choose(m, tag);
                break;
            case 0x4b: /* left */
                if (omni)
                    move_to(m, nearest(m, m->citem, D_LEFT));
                else if (horz)
                    move_to(m, step(m, m->citem, 0));
                else if (m->menutype & M_PD)
                    r = A_PREV;
                break;
            case 0x4d: /* right */
                if (omni)
                    move_to(m, nearest(m, m->citem, D_RIGHT));
                else if (horz)
                    move_to(m, step(m, m->citem, 1));
                else if (m->menutype & M_PD)
                    r = A_NEXT;
                break;
            case 0x47: /* home */
                i = m->item;
                move_to(m, selectable(i) ? i : step(m, i, 1));
                break;
            case 0x4f: /* end */
                i = last_item(m);
                move_to(m, selectable(i) ? i : step(m, i, 0));
                break;
            default:
                r = hotkey(m, key, tag);
                if (r == -2)
                    r = -1;
            }
        } else {
            r = hotkey(m, key, tag);
            if (r == -2 && !(m->menutype & M_NOQS)) {
                r = -1;
                for (i = m->item; i; i = i->next)
                    if (selectable(i) && i->schar &&
                        toupper(i->schar) == toupper(c))
                        break;
                if (i) {
                    move_to(m, i);
                    r = choose(m, tag);
                }
            }
        }
        if (r >= 0) {
            act = r;
            break;
        }
    }
    close_menu(m);
out:
    _winfo.mlevel--;
    _winfo.cmenu = savecm;
    _kbinfo.inmenu = _winfo.mlevel > 0;
    return act;
}

int wmenuget(void)
{
    struct _menu_t *m = _winfo.menu;
    int tag = -1, act;
    if (!m)
        return _cxl_werr(W_NOMNUDEF), -1;
    if (defm || !m->_ended)
        return _cxl_werr(W_NOMNUEND), -1;
    act = run(m, &tag);
    /* The definition is used up: unlink and free it. */
    _winfo.menu = m->prev;
    if (_winfo.menu)
        _winfo.menu->next = NULL;
    free_menu(m);
    switch (act) {
    case A_SELECT:
    case A_ALL:
        _cxl_werr(W_NOERROR);
        return tag;
    case A_ESC:
        _cxl_werr(W_ESCPRESS);
        return -1;
    }
    return -1;
}
