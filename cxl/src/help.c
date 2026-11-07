/* CXL compatible library: context-sensitive help.
 *
 * Help files are plain text. A category starts with a line
 * "*B number[,name]", "*P" breaks pages and "*E" ends it. "^name^" is a
 * cross-reference to the category with that name and "^^" is a caret.
 * Files indexed by other tools (starting with "*I") work too, because the
 * text is searched line by line.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <ctype.h>
#include <stdio.h>
#include "cxl-local.h"

#define MAXTEXT 8192
#define MAXPAGES 32
#define MAXREFS 40

static struct _help_t *H(void)
{
    return _winfo.helptr;
}

static int busy;

static int hook(unsigned key);

int whelpdef(char *file, unsigned key, int winattr, int textattr, int selattr,
             int barattr, void (*open)(void))
{
    struct _help_t *h = _winfo.helptr;
    if (!file) {
        if (h) {
            free(h->file);
            free(h);
        }
        _winfo.helptr = NULL;
        _cxl_helphook = NULL;
        return _cxl_werr(W_NOHLPDEF);
    }
    if (!h) {
        h = calloc(1, sizeof *h);
        if (!h)
            return _cxl_werr(W_ALLOCERR);
        h->helpptr = -1;
        h->srow = (_vinfo.numrows - 19) / 2;
        h->erow = h->srow + 18;
        h->scol = (_vinfo.numcols - 64) / 2;
        h->ecol = h->scol + 63;
        h->btype = 0;
        h->title = 1;
    }
    free(h->file);
    h->file = malloc(strlen(file) + 1);
    if (!h->file) {
        free(h);
        _winfo.helptr = NULL;
        return _cxl_werr(W_ALLOCERR);
    }
    strcpy(h->file, file);
    h->key = key;
    h->winattr = winattr;
    h->textattr = textattr;
    h->selattr = selattr;
    h->barattr = barattr;
    h->open = open;
    _winfo.helptr = h;
    _cxl_helphook = hook;
    return _cxl_werr(W_NOERROR);
}

int whelpwin(int srow, int scol, int erow, int ecol, int btype, int title)
{
    struct _help_t *h = H();
    if (!h)
        return _cxl_werr(W_NOHLPDEF);
    h->srow = srow;
    h->scol = scol;
    h->erow = erow;
    h->ecol = ecol;
    h->btype = btype;
    h->title = title;
    return _cxl_werr(W_NOERROR);
}

int whelpcat(int cat)
{
    _winfo.help = cat;
    if (_winfo.active)
        _winfo.active->help = cat;
    return _cxl_werr(H() ? W_NOERROR : W_NOHLPDEF);
}

int whelpclr(void)
{
    if (!H())
        return _cxl_werr(W_NOHLPDEF);
    H()->helpptr = -1;
    return _cxl_werr(W_NOERROR);
}

int whelpushc(int cat)
{
    struct _help_t *h = H();
    if (!h)
        return _cxl_werr(W_NOHLPDEF);
    if (h->helpptr >= 19)
        return _cxl_werr(W_HLPSTKOV);
    h->help[++h->helpptr] = cat;
    return _cxl_werr(W_NOERROR);
}

int whelpush(void)
{
    return whelpushc(_winfo.help);
}

int whelppcat(int cat)
{
    int e = whelpush();
    if (e)
        return e;
    return whelpcat(cat);
}

int whelpopc(void)
{
    struct _help_t *h = H();
    if (!h)
        return _cxl_werr(W_NOHLPDEF);
    if (h->helpptr < 0)
        return _cxl_werr(W_HLPSTKUN);
    h->helpptr--;
    return _cxl_werr(W_NOERROR);
}

int whelpop(void)
{
    struct _help_t *h = H();
    if (!h)
        return _cxl_werr(W_NOHLPDEF);
    if (h->helpptr < 0)
        return _cxl_werr(W_HLPSTKUN);
    _winfo.help = h->help[h->helpptr--];
    return _cxl_werr(W_NOERROR);
}

/* Reading the help file. */

static int marker(const char *line, int c)
{
    return line[0] == '*' && toupper((unsigned char)line[1]) == c &&
           (line[2] == 0 || line[2] == ' ' || line[2] == '\r' ||
            line[2] == '\n' || line[2] == ',');
}

static void chomp(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = 0;
}

/* Finds a category by number, or by name if name is not NULL. Leaves the
 * file just past its *B line and returns its number, or -1. */
static int seek_cat(FILE *f, int cat, const char *name)
{
    char line[160];
    rewind(f);
    while (fgets(line, sizeof line, f)) {
        if (marker(line, 'B')) {
            char *p = line + 2, *q;
            int n;
            while (*p == ' ')
                p++;
            n = atoi(p);
            if (!name) {
                if (n == cat)
                    return n;
                continue;
            }
            q = strchr(p, ',');
            if (q) {
                chomp(++q);
                while (*q == ' ')
                    q++;
                if (!strcasecmp(q, name))
                    return n;
            }
        }
    }
    return -1;
}

struct page {
    char *text;
    int start[MAXPAGES];
    int npages;
};

static int read_cat(FILE *f, struct page *pg)
{
    char line[160];
    int len = 0;
    pg->npages = 1;
    pg->start[0] = 0;
    while (fgets(line, sizeof line, f)) {
        int n;
        if (marker(line, 'E') || marker(line, 'B'))
            break;
        if (marker(line, 'P')) {
            if (pg->npages < MAXPAGES)
                pg->start[pg->npages++] = len;
            continue;
        }
        chomp(line);
        n = strlen(line);
        if (len + n + 2 >= MAXTEXT)
            break;
        memcpy(pg->text + len, line, n);
        len += n;
        pg->text[len++] = '\n';
    }
    pg->text[len] = 0;
    pg->start[pg->npages] = len;
    return len;
}

struct xref {
    int row, col, len;
    char name[40];
};

/* Draws one page; returns the number of cross-references on it. */
static int draw_page(struct page *pg, int n, struct xref *x)
{
    struct _help_t *h = H();
    const char *p = pg->text + pg->start[n], *end = pg->text + pg->start[n + 1];
    struct _wrec_t *w = _winfo.active;
    int nx = 0;
    char buf[2] = {0, 0};

    wcclear(h->winattr);
    w->attr = h->textattr;
    while (p < end) {
        if (*p == '^') {
            if (p[1] == '^') {
                _cxl_wtty('^');
                p += 2;
                continue;
            }
            {
                const char *q = memchr(p + 1, '^', end - p - 1);
                int len;
                if (!q) {
                    _cxl_wtty('^');
                    p++;
                    continue;
                }
                len = q - p - 1;
                if (nx < MAXREFS && len < (int)sizeof x[nx].name) {
                    x[nx].row = w->row;
                    x[nx].col = w->column;
                    x[nx].len = len;
                    memcpy(x[nx].name, p + 1, len);
                    x[nx].name[len] = 0;
                    nx++;
                }
                w->attr = h->selattr;
                for (p++; p < q; p++)
                    _cxl_wtty((unsigned char)*p);
                w->attr = h->textattr;
                p = q + 1;
                continue;
            }
        }
        if (*p == 0x1b && p + 1 < end) {
            /* An escape code, with its parameter bytes. */
            int k = 2 + (strchr("AFBRCE", p[1]) ? 1 : p[1] == 'D' ? 2 : 0);
            char tmp[5];
            if (p + k > end)
                k = end - p;
            memcpy(tmp, p, k);
            tmp[k] = 0;
            _cxl_wttys(tmp, 0);
            p += k;
            continue;
        }
        buf[0] = *p++;
        _cxl_wttys(buf, 0);
    }
    if (pg->npages > 1 && w->border) {
        char msg[24];
        snprintf(msg, sizeof msg, " %d of %d ", n + 1, pg->npages);
        wmessage(msg, BT_BORD, w->ecol - w->scol - (int)strlen(msg) - 1,
                 w->battr);
    }
    return nx;
}

static void bar(struct xref *x, int on)
{
    struct _help_t *h = H();
    int i;
    for (i = 0; i < x->len; i++)
        wprintc(x->row, x->col + i, on ? h->barattr : h->selattr,
                (unsigned char)x->name[i]);
}

static void show(int cat)
{
    struct _help_t *h = H();
    struct _onkey_t *keys;
    struct page pg;
    struct xref x[MAXREFS];
    FILE *f;
    int page = 0, nx, sel = 0;
    int saverow, savecol, cs, ce;

    if (!cat && h->helpptr >= 0)
        cat = h->help[h->helpptr];
    f = fopen(h->file, "rb");
    if (!f) {
        wperror("Help file not found");
        return;
    }
    pg.text = malloc(MAXTEXT);
    if (!pg.text) {
        fclose(f);
        return;
    }
    _cxl_getcur(&saverow, &savecol);
    getcursz(&cs, &ce);
    keys = chgonkey(NULL);
    if (!wopen(h->srow, h->scol, h->erow, h->ecol, h->btype, h->winattr,
               h->winattr)) {
        chgonkey(keys);
        free(pg.text);
        fclose(f);
        return;
    }
    if (h->title)
        wtitle(" Help ", TCENTER, h->winattr);
    if (h->open)
        h->open();
    hidecur();
    if (seek_cat(f, cat, NULL) < 0) {
        strcpy(pg.text, "No help is available here.\n");
        pg.npages = 1;
        pg.start[0] = 0;
        pg.start[1] = strlen(pg.text);
    } else {
        read_cat(f, &pg);
    }
    nx = draw_page(&pg, page, x);
    if (nx)
        bar(&x[0], 1);
    for (;;) {
        unsigned key = getxch();
        int c = key & 0xff, sc = key >> 8;
        if (c == 0x1b)
            break;
        if (c == 0 && (sc == 0x51 || sc == 0x49)) { /* PgDn, PgUp */
            int np = page + (sc == 0x51 ? 1 : -1);
            if (np >= 0 && np < pg.npages) {
                page = np;
                sel = 0;
                nx = draw_page(&pg, page, x);
                if (nx)
                    bar(&x[0], 1);
            }
            continue;
        }
        if (!nx)
            continue;
        if (c == '\t' || (c == 0 && (sc == 0x4d || sc == 0x50)) ||
            (c == 0 && (sc == 0x0f || sc == 0x4b || sc == 0x48))) {
            int fwd = c == '\t' || sc == 0x4d || sc == 0x50;
            bar(&x[sel], 0);
            sel = (sel + (fwd ? 1 : nx - 1)) % nx;
            bar(&x[sel], 1);
        } else if (c == '\r') {
            int n = seek_cat(f, 0, x[sel].name);
            if (n >= 0 && seek_cat(f, n, NULL) >= 0) {
                read_cat(f, &pg);
                page = 0;
                sel = 0;
                nx = draw_page(&pg, page, x);
                if (nx)
                    bar(&x[0], 1);
            }
        }
    }
    wclose();
    freonkey(); /* keys the open function set */
    chgonkey(keys);
    _cxl_setcur(saverow, savecol);
    setcursz(cs, ce);
    free(pg.text);
    fclose(f);
}

static int hook(unsigned key)
{
    if (!H() || key != H()->key || busy)
        return 0;
    busy = 1;
    show(_winfo.help);
    busy = 0;
    return 1;
}

int whelpget(void)
{
    if (!H())
        return _cxl_werr(W_NOHLPDEF);
    if (!busy) {
        busy = 1;
        show(_winfo.help);
        busy = 0;
    }
    return _cxl_werr(W_NOERROR);
}
