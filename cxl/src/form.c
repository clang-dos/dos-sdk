/* CXL compatible library: data-entry forms (winpbeg, winpdef, winpread).
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <ctype.h>
#include "cxl-local.h"

static char defsep[] = " ~`!@#$%^&*()-+=|\\[]{}:;'\",./<>?";
char *_cxl_wordsep = defsep;

#define W (_winfo.active)

static struct _form_t *cur_form(void)
{
    return _winfo.active ? _winfo.active->form : NULL;
}

int winpbeg(int fieldattr, int textattr)
{
    struct _form_t *f;
    if (_cxl_wbeg())
        return _winfo.errno;
    f = calloc(1, sizeof *f);
    if (!f)
        return _cxl_werr(W_ALLOCERR);
    f->fieldattr = fieldattr;
    f->textattr = textattr;
    f->prev = W->form;
    if (f->prev)
        f->prev->next = f;
    W->form = f;
    return _cxl_werr(W_NOERROR);
}

int winpdef(int wrow, int wcol, char *str, char *format, int fconv, int mode,
            int (*validate)(char *), int help)
{
    struct _form_t *f = cur_form();
    struct _field_t *d, *t;
    struct _cxl_slot *sl;
    int n, i, inputs = 0, dec = -1;

    if (_cxl_wbeg())
        return _winfo.errno;
    if (!f)
        return _cxl_werr(W_NOFRMBEG);
    n = _cxl_fmtparse(format, 0, 0, &sl);
    if (n <= 0)
        return _cxl_werr(W_INVFORMT);
    for (i = 0; i < n; i++) {
        if (sl[i].kind < SL_LIT)
            inputs++;
        else if (sl[i].kind == SL_DECIMAL && dec < 0)
            dec = inputs;
    }
    if (!inputs) {
        free(sl);
        return _cxl_werr(W_INVFORMT);
    }
    if (wchkcoord(wrow, wcol) || wchkcol(wcol + n - 1)) {
        free(sl);
        return _cxl_werr(W_INVCOORD);
    }
    d = calloc(1, sizeof *d);
    if (d) {
        d->buf = malloc(inputs + 1);
        d->_orig = malloc(strlen(str) + 1);
    }
    if (!d || !d->buf || !d->_orig) {
        if (d) {
            free(d->buf);
            free(d->_orig);
        }
        free(d);
        free(sl);
        return _cxl_werr(W_ALLOCERR);
    }
    strcpy(d->_orig, str);
    d->str = str;
    d->format = format;
    d->validate = validate;
    d->help = help;
    d->lenbuf = inputs;
    d->lenfld = n;
    d->lenfmt = strlen(format);
    d->wrow = wrow;
    d->wcol = wcol;
    d->mode = mode;
    d->decpos = dec < 0 ? inputs : dec;
    d->fconv = fconv;
    d->_slots = sl;
    d->_nslots = n;
    d->redisp = 1;
    t = f->field;
    while (t && t->next)
        t = t->next;
    if (t) {
        t->next = d;
        d->prev = t;
    } else {
        f->field = d;
    }
    f->cfield = d;
    if (dec >= 0)
        f->decimal = 1;
    return _cxl_werr(W_NOERROR);
}

int winpfba(void (*before)(void), void (*after)(void))
{
    struct _form_t *f = cur_form();
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!f)
        return _cxl_werr(W_NOFRMBEG);
    if (!f->cfield)
        return _cxl_werr(W_NOINPDEF);
    f->cfield->before = before;
    f->cfield->after = after;
    return _cxl_werr(W_NOERROR);
}

int winpkey(unsigned (*getkey)(int *), unsigned *termkey)
{
    struct _form_t *f = cur_form();
    if (_cxl_wbeg())
        return _winfo.errno;
    if (!f)
        return _cxl_werr(W_NOFRMBEG);
    f->getkey = getkey;
    f->termkey = termkey;
    return _cxl_werr(W_NOERROR);
}

int winpwordsep(char *sepstr)
{
    _cxl_wordsep = sepstr ? sepstr : defsep;
    return _cxl_werr(W_NOERROR);
}

struct _field_t *winpffind(int wrow, int wcol)
{
    struct _form_t *f = cur_form();
    struct _field_t *d;
    if (_cxl_wbeg())
        return NULL;
    if (!f) {
        _cxl_werr(W_NOFRMDEF);
        return NULL;
    }
    for (d = f->field; d; d = d->next)
        if (d->wrow == wrow && d->wcol == wcol) {
            _cxl_werr(W_NOERROR);
            return d;
        }
    _cxl_werr(W_NOTFOUND);
    return NULL;
}

/* Editing. */

static struct _cxl_slot *slots(struct _field_t *d)
{
    return d->_slots;
}

/* Screen column offset of input position p. */
static int col_of(struct _field_t *d, int p)
{
    int j, k = 0;
    for (j = 0; j < d->_nslots; j++)
        if (slots(d)[j].kind < SL_LIT) {
            if (k == p)
                return j;
            k++;
        }
    return d->_nslots - 1;
}

static struct _cxl_slot *slot_of(struct _field_t *d, int p)
{
    return &slots(d)[col_of(d, p)];
}

static int is_password(struct _field_t *d, int p)
{
    return d->fconv == 'P' || slot_of(d, p)->ch == 'P';
}

static void show(struct _form_t *f, struct _field_t *d)
{
    int j, k = 0;
    for (j = 0; j < d->_nslots; j++) {
        struct _cxl_slot *s = &slots(d)[j];
        int ch, at = f->textattr;
        if (s->kind < SL_LIT) {
            ch = (unsigned char)d->buf[k];
            if (is_password(d, k) && ch != ' ')
                ch = ' ';
            k++;
            at = f->fieldattr;
        } else if (s->kind == SL_DECIMAL) {
            ch = '.';
        } else {
            ch = s->ch;
        }
        wprintc(d->wrow, d->wcol + j, at, ch);
    }
    d->redisp = 0;
}

static void place_cursor(struct _field_t *d, int p)
{
    if (p >= d->lenbuf)
        p = d->lenbuf - 1;
    wgotoxy(d->wrow, d->wcol + col_of(d, p));
}

static void load(struct _field_t *d)
{
    size_t n = strlen(d->str);
    memset(d->buf, ' ', d->lenbuf);
    d->buf[d->lenbuf] = 0;
    if (d->mode != 0)
        memcpy(d->buf, d->str, n < (size_t)d->lenbuf ? n : (size_t)d->lenbuf);
}

static int data_end(struct _field_t *d)
{
    int e = d->lenbuf;
    while (e > 0 && d->buf[e - 1] == ' ')
        e--;
    return e < d->lenbuf ? e : d->lenbuf - 1;
}

/* Numeric fields: whole part right justified, fraction zero filled. */
static void numeric(struct _field_t *d)
{
    char tmp[128];
    int w = d->decpos, i, k = 0;
    if (w > (int)sizeof tmp - 1)
        return;
    for (i = 0; i < w; i++)
        if (d->buf[i] != ' ')
            tmp[k++] = d->buf[i];
    memset(d->buf, ' ', w - k);
    memcpy(d->buf + w - k, tmp, k);
    k = 0;
    for (i = w; i < d->lenbuf; i++)
        if (d->buf[i] != ' ')
            tmp[k++] = d->buf[i];
    for (i = 0; i < d->lenbuf - w; i++)
        d->buf[w + i] = i < k ? tmp[i] : '0';
}

static int is_sep(int c)
{
    return c == ' ' || (c && strchr(_cxl_wordsep, c));
}

static int blank_from(struct _field_t *d, int p)
{
    for (; p < d->lenbuf; p++)
        if (d->buf[p] != ' ')
            return 0;
    return 1;
}

static void del_at(struct _field_t *d, int p)
{
    if (p < 0 || p >= d->lenbuf)
        return;
    memmove(d->buf + p, d->buf + p + 1, d->lenbuf - p - 1);
    d->buf[d->lenbuf - 1] = ' ';
}

/* Leaves the field: conversion, copy out, validation and the after
 * function. Returns 0, or the 1-based position the validator rejected. */
static int leave(struct _form_t *f, struct _field_t *d)
{
    int e = 0;
    if (d->fconv == '9')
        numeric(d);
    strcpy(d->str, d->buf);
    if (d->validate) {
        e = d->validate(d->str);
        if (e) {
            load(d);
            if (d->mode == 0)
                memcpy(d->buf, d->str, strlen(d->str) < (size_t)d->lenbuf
                                           ? strlen(d->str) : (size_t)d->lenbuf);
        }
    }
    show(f, d);
    if (e)
        return e;
    if (d->after) {
        d->after();
    }
    return 0;
}

static struct _field_t *last_field(struct _form_t *f)
{
    struct _field_t *d = f->field;
    while (d && d->next)
        d = d->next;
    return d;
}

static struct _field_t *vertical(struct _form_t *f, struct _field_t *c, int down)
{
    struct _field_t *d, *best = NULL;
    long bs = 0;
    for (d = f->field; d; d = d->next) {
        long dr = (long)d->wrow - c->wrow, dc = (long)d->wcol - c->wcol, s;
        if (down ? dr <= 0 : dr >= 0)
            continue;
        s = (dr < 0 ? -dr : dr) * 256 + (dc < 0 ? -dc : dc);
        if (!best || s < bs) {
            best = d;
            bs = s;
        }
    }
    return best;
}

/* Before the form is accepted: copies every field out and validates the
 * ones other than cur (already done). Returns the first bad field. */
static struct _field_t *final_check(struct _form_t *f, struct _field_t *cur)
{
    struct _field_t *d;
    for (d = f->field; d; d = d->next) {
        if (d == cur)
            continue;
        if (d->fconv == '9')
            numeric(d);
        strcpy(d->str, d->buf);
        if (d->validate && d->validate(d->str))
            return d;
    }
    return NULL;
}

static void free_form(struct _wrec_t *w, struct _form_t *f)
{
    struct _field_t *d = f->field;
    while (d) {
        struct _field_t *n = d->next;
        free(d->buf);
        free(d->_slots);
        free(d->_orig);
        free(d);
        d = n;
    }
    w->form = f->prev;
    if (w->form)
        w->form->next = NULL;
    free(f);
}

int winpread(void)
{
    struct _wrec_t *w = _winfo.active;
    struct _form_t *f = cur_form();
    struct _field_t *d, *go;
    int p = 0, fresh = 1, cond = 0, done, result = W_NOERROR, cs, ce;
    unsigned key = 0;

    if (_cxl_wbeg())
        return _winfo.errno;
    if (!f || !f->field) {
        if (f)
            free_form(w, f);
        return _cxl_werr(W_NOINPDEF);
    }
    for (d = f->field; d; d = d->next) {
        load(d);
        show(f, d);
    }
    d = f->field;
    /* Show the cursor while the form runs, and put it back afterwards. */
    getcursz(&cs, &ce);
    if (cs & 0x20)
        showcur();

    for (;;) {
        int ch;
        if (fresh) {
            f->cfield = d;
            _winfo.help = d->help;
            cond = d->mode == 2;
            p = cond ? data_end(d) : 0;
            if (d->before)
                d->before();
            fresh = 0;
        }
        for (go = f->field; go; go = go->next)
            if (go->redisp)
                show(f, go);
        place_cursor(d, p);

        done = 0;
        key = f->getkey ? f->getkey(&done) : getxch();
        ch = key & 0xff;
        go = NULL;

        if (ch == 0x1b && _winfo.esc) {
            for (d = f->field; d; d = d->next)
                strcpy(d->str, d->_orig);
            result = W_ESCPRESS;
            break;
        }
        if (cond && ch >= ' ' && key != 0x0e7f) {
            memset(d->buf, ' ', d->lenbuf);
            p = 0;
            show(f, d);
        }
        cond = 0;

        if (key == 0x1c0a || done) { /* Ctrl-Enter: accept the form */
            int e = leave(f, d);
            if (e) {
                p = e - 1 < d->lenbuf ? e - 1 : 0;
                continue;
            }
            go = final_check(f, d);
            if (go) {
                d = go;
                fresh = 1;
                continue;
            }
            break;
        }
        switch (ch) {
        case '\r':
            go = d->next;
            if (!go) {
                int e = leave(f, d);
                if (e) {
                    p = e - 1 < d->lenbuf ? e - 1 : 0;
                    continue;
                }
                go = final_check(f, d);
                if (go) {
                    d = go;
                    fresh = 1;
                    continue;
                }
                goto out;
            }
            break;
        case '\t':
            go = d->next ? d->next : f->field;
            break;
        case '\b':
            if (p > 0) {
                p--;
                del_at(d, p);
                show(f, d);
            }
            continue;
        case 0x7f: /* Ctrl-Backspace: word left */
            while (p > 0 && is_sep((unsigned char)d->buf[p - 1])) {
                p--;
                del_at(d, p);
            }
            while (p > 0 && !is_sep((unsigned char)d->buf[p - 1])) {
                p--;
                del_at(d, p);
            }
            show(f, d);
            continue;
        case 0x12: /* Ctrl-R: restore */
            load(d);
            if (d->mode == 0) {
                size_t n = strlen(d->_orig);
                memcpy(d->buf, d->_orig, n < (size_t)d->lenbuf ? n : (size_t)d->lenbuf);
            }
            p = 0;
            show(f, d);
            continue;
        case 0x14: /* Ctrl-T: delete word right */
            while (p < d->lenbuf && !is_sep((unsigned char)d->buf[p]))
                del_at(d, p);
            while (p < d->lenbuf && is_sep((unsigned char)d->buf[p]) &&
                   !blank_from(d, p))
                del_at(d, p);
            show(f, d);
            continue;
        case 0x15: /* Ctrl-U: delete to end of field */
            memset(d->buf + p, ' ', d->lenbuf - p);
            show(f, d);
            continue;
        case 0x19: { /* Ctrl-Y: delete to end of the last field */
            struct _field_t *x;
            memset(d->buf + p, ' ', d->lenbuf - p);
            show(f, d);
            for (x = d->next; x; x = x->next) {
                memset(x->buf, ' ', x->lenbuf);
                show(f, x);
            }
            continue;
        }
        case 0:
            switch (key >> 8) {
            case 0x4b: if (p > 0) p--; continue;               /* left */
            case 0x4d: if (p < d->lenbuf - 1) p++; continue;   /* right */
            case 0x48: go = vertical(f, d, 0); break;           /* up */
            case 0x50: go = vertical(f, d, 1); break;           /* down */
            case 0x0f: go = d->prev ? d->prev : last_field(f); break; /* shift-tab */
            case 0x47: p = 0; continue;                         /* home */
            case 0x4f: p = data_end(d); continue;               /* end */
            case 0x77: go = f->field; break;                    /* ctrl-home */
            case 0x75: go = last_field(f); break;               /* ctrl-end */
            case 0x52: f->insert = !f->insert;                  /* ins */
                if (f->insert)
                    lgcursor();
                else
                    smcursor();
                continue;
            case 0x53: del_at(d, p); show(f, d); continue;      /* del */
            case 0x73: /* ctrl-left */
                while (p > 0 && is_sep((unsigned char)d->buf[p - 1]))
                    p--;
                while (p > 0 && !is_sep((unsigned char)d->buf[p - 1]))
                    p--;
                continue;
            case 0x74: /* ctrl-right */
                while (p < d->lenbuf - 1 && !is_sep((unsigned char)d->buf[p]))
                    p++;
                while (p < d->lenbuf - 1 && is_sep((unsigned char)d->buf[p]))
                    p++;
                continue;
            }
            continue;
        default:
            if (ch == '.' && d->fconv == '9' && d->decpos < d->lenbuf) {
                p = d->decpos;
                continue;
            }
            if (ch >= ' ') {
                int prev = p > 0 ? (unsigned char)d->buf[p - 1] : 0;
                int c = _cxl_slotok(slot_of(d, p), ch, prev, d->fconv);
                if (c < 0) {
                    beep();
                    continue;
                }
                if (f->insert) {
                    memmove(d->buf + p + 1, d->buf + p, d->lenbuf - p - 1);
                }
                d->buf[p++] = c;
                show(f, d);
                if (p >= d->lenbuf) {
                    go = d->next;
                    if (!go) {
                        int e = leave(f, d);
                        if (e) {
                            p = e - 1 < d->lenbuf ? e - 1 : 0;
                            continue;
                        }
                        go = final_check(f, d);
                        if (go) {
                            d = go;
                            fresh = 1;
                            continue;
                        }
                        goto out;
                    }
                    break;
                }
            }
            continue;
        }
        if (go && go != d) {
            int e = leave(f, d);
            if (e) {
                p = e - 1 < d->lenbuf ? e - 1 : 0;
                continue;
            }
            d = go;
            fresh = 1;
        }
    }
out:
    setcursz(cs, ce);
    if (f->termkey)
        *f->termkey = key;
    free_form(w, f);
    return _cxl_werr(result);
}
