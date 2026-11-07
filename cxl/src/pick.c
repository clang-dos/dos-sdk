/* CXL compatible library: pick lists (wpickstr, wpickfile, wselstr,
 * wpickattr).
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <ctype.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cxl-local.h"

#define W (_winfo.active)

struct grid {
    char **items;
    int n, ncols, colw, rows, top, sel, wattr, barattr;
};

static void draw_cell(struct grid *g, int i)
{
    int r = i / g->ncols - g->top, c = (i % g->ncols) * g->colw, k;
    const char *s = g->items[i];
    int at = i == g->sel ? g->barattr : g->wattr;
    if (r < 0 || r >= g->rows)
        return;
    for (k = 0; k < g->colw; k++) {
        int ch = ' ';
        if (k >= 1 && *s)
            ch = (unsigned char)*s++;
        wprintc(r, c + k, at, ch);
    }
}

static void draw_grid(struct grid *g)
{
    int r, i;
    for (r = 0; r < g->rows; r++)
        for (i = 0; i < g->ncols; i++) {
            int idx = (g->top + r) * g->ncols + i;
            if (idx < g->n)
                draw_cell(g, idx);
            else
                wfill(r, i * g->colw, r, i * g->colw + g->colw - 1, ' ', g->wattr);
        }
}

static void move_sel(struct grid *g, int to)
{
    int old = g->sel, row;
    if (to < 0)
        to = 0;
    if (to >= g->n)
        to = g->n - 1;
    if (to == old)
        return;
    g->sel = to;
    row = to / g->ncols;
    if (row < g->top || row >= g->top + g->rows) {
        g->top = row < g->top ? row : row - g->rows + 1;
        draw_grid(g);
    } else {
        draw_cell(g, old);
        draw_cell(g, to);
    }
}

/* Runs the list in the active window. Returns the index or -1. */
static int grid_run(struct grid *g)
{
    for (;;) {
        unsigned key = getxch();
        int c = key & 0xff, page = g->ncols * g->rows, i;
        if (c == 0x1b) {
            if (_winfo.esc) {
                _cxl_werr(W_ESCPRESS);
                return -1;
            }
            continue;
        }
        if (c == '\r') {
            if (_kbinfo.source == 2 && (_mouse & MS_CURS)) {
                int r = _cxl_mrow - _cxl_wrow0(W), col = _cxl_mcol - _cxl_wcol0(W);
                if (r < 0 || col < 0 || r >= g->rows || col >= g->ncols * g->colw)
                    continue;
                i = (g->top + r) * g->ncols + col / g->colw;
                if (i >= g->n)
                    continue;
                move_sel(g, i);
            }
            _cxl_werr(W_NOERROR);
            return g->sel;
        }
        if (c == 0) {
            switch (key >> 8) {
            case 0x48: move_sel(g, g->sel - g->ncols); break;
            case 0x50: move_sel(g, g->sel + g->ncols); break;
            case 0x4b: move_sel(g, g->sel - 1); break;
            case 0x4d: move_sel(g, g->sel + 1); break;
            case 0x49: move_sel(g, g->sel - page); break;
            case 0x51: move_sel(g, g->sel + page); break;
            case 0x47: move_sel(g, 0); break;
            case 0x4f: move_sel(g, g->n - 1); break;
            }
            continue;
        }
        /* A letter jumps to the next item that starts with it. */
        for (i = 1; i <= g->n; i++) {
            int k = (g->sel + i) % g->n;
            if (toupper((unsigned char)g->items[k][0]) == toupper(c)) {
                move_sel(g, k);
                break;
            }
        }
    }
}

/* Opens the pick window, runs the list and closes the window. */
static int pick(int srow, int scol, int erow, int ecol, int btype, int bordattr,
                int winattr, int barattr, char **items, int n, int init,
                void (*open)(void), const char *title)
{
    struct grid g;
    int maxlen = 0, i, b = btype != 5, r, cs, ce;
    for (i = 0; i < n; i++) {
        int l = strlen(items[i]);
        if (l > maxlen)
            maxlen = l;
    }
    if (btype < 0 || btype > 5)
        return _cxl_werr(W_INVBTYPE), -1;
    if (ecol == -1)
        ecol = scol + maxlen + 1 + 2 * b;
    if (erow == -1)
        erow = srow + n - 1 + 2 * b;
    if (ecol >= _vinfo.numcols)
        ecol = _vinfo.numcols - 1;
    if (erow >= _vinfo.numrows)
        erow = _vinfo.numrows - 1;
    if (ecol - scol + 1 - 2 * b < maxlen + 2 && ecol - scol + 1 - 2 * b < maxlen)
        return _cxl_werr(W_STRLONG), -1;
    if (!wopen(srow, scol, erow, ecol, btype, bordattr, winattr))
        return -1;
    if (title)
        wtitle((char *)title, TCENTER, bordattr);
    if (open)
        open();
    g.items = items;
    g.n = n;
    g.colw = maxlen + 2;
    g.ncols = _cxl_wcols(W) / g.colw;
    if (g.ncols < 1)
        g.ncols = 1;
    if (g.ncols == 1)
        g.colw = _cxl_wcols(W);
    g.rows = _cxl_wrows(W);
    g.sel = (init >= 0 && init < n) ? init : 0;
    g.top = g.sel / g.ncols >= g.rows ? g.sel / g.ncols - g.rows + 1 : 0;
    g.wattr = winattr;
    g.barattr = barattr;
    getcursz(&cs, &ce);
    hidecur();
    draw_grid(&g);
    _kbinfo.inmenu = 1;
    r = grid_run(&g);
    _kbinfo.inmenu = 0;
    i = _winfo.errno;
    setcursz(cs, ce);
    wclose();
    _winfo.errno = i;
    return r;
}

int wpickstr(int srow, int scol, int erow, int ecol, int btype, int bordattr,
             int winattr, int barattr, char *strarr[], int initelem,
             void (*open)(void))
{
    int n = 0;
    while (strarr[n])
        n++;
    if (!n)
        return _cxl_werr(W_NOITMDEF), -1;
    return pick(srow, scol, erow, ecol, btype, bordattr, winattr, barattr,
                strarr, n, initelem, open, NULL);
}

static int cmp_names(const void *a, const void *b)
{
    const char *x = *(char *const *)a, *y = *(char *const *)b;
    int dx = x[strlen(x) - 1] == '\\', dy = y[strlen(y) - 1] == '\\';
    if (dx != dy)
        return dy - dx; /* directories first */
    return strcasecmp(x, y);
}

static void upcase(char *s)
{
    for (; *s; s++)
        *s = toupper((unsigned char)*s);
}

char *wpickfile(int srow, int scol, int erow, int ecol, int btype,
                int bordattr, int winattr, int barattr, int title,
                char *filespec, void (*open)(void))
{
    static char result[80];
    char spec[80], cwd[68], titlebuf[80];
    char *slash;

    strncpy(spec, filespec, sizeof spec - 1);
    spec[sizeof spec - 1] = 0;
    slash = strrchr(spec, '\\');
    if (!slash)
        slash = strrchr(spec, '/');
    if (!slash && spec[0] && spec[1] == ':')
        slash = spec + 1;
    if (slash) {
        char dir[80];
        size_t n = slash - spec + 1;
        memcpy(dir, spec, n);
        dir[n] = 0;
        if (n > 1 && dir[n - 1] != ':')
            dir[n - 1] = 0;
        if (dir[1] == ':' && dir[2] == 0) {
            union REGS r;
            r.h.ah = 0x0e;
            r.h.dl = toupper((unsigned char)dir[0]) - 'A';
            intdos(&r, &r);
        } else if (chdir(dir)) {
            _cxl_werr(W_DOSERROR);
            return NULL;
        }
        memmove(spec, slash + 1, strlen(slash + 1) + 1);
    }
    upcase(spec);

    for (;;) {
        DIR *d;
        struct dirent *e;
        char **names;
        int n = 0, max = _winfo.maxfiles, i, files = 0, sel;

        names = malloc(max * sizeof(char *));
        if (!names) {
            _cxl_werr(W_ALLOCERR);
            return NULL;
        }
        d = opendir(".");
        if (!d) {
            free(names);
            _cxl_werr(W_DOSERROR);
            return NULL;
        }
        while ((e = readdir(d)) != NULL && n < max) {
            char name[16];
            int isdir;
            struct stat st;
            if (!strcmp(e->d_name, "."))
                continue;
            strncpy(name, e->d_name, 13);
            name[13] = 0;
            upcase(name);
            isdir = e->d_type == DT_DIR;
            if (e->d_type == DT_UNKNOWN && !stat(e->d_name, &st))
                isdir = S_ISDIR(st.st_mode);
            if (!isdir && fnmatch(spec, name, 0))
                continue;
            if (isdir)
                strcat(name, "\\");
            else
                files++;
            names[n] = malloc(strlen(name) + 1);
            if (!names[n])
                break;
            strcpy(names[n++], name);
        }
        closedir(d);
        if (!getcwd(cwd, sizeof cwd))
            strcpy(cwd, "");
        if (!files && n == 0) {
            free(names);
            _cxl_werr(W_NOMATCH);
            return NULL;
        }
        qsort(names, n, sizeof(char *), cmp_names);
        snprintf(titlebuf, sizeof titlebuf, " %s%s%s ", cwd,
                 (cwd[0] && cwd[strlen(cwd) - 1] != '\\') ? "\\" : "", spec);
        sel = pick(srow, scol, erow, ecol, btype, bordattr, winattr, barattr,
                   names, n, 0, open, title ? titlebuf : NULL);
        if (sel >= 0) {
            char *nm = names[sel];
            size_t l = strlen(nm);
            if (nm[l - 1] == '\\') {
                nm[l - 1] = 0;
                if (chdir(nm))
                    sel = -2;
            } else {
                snprintf(result, sizeof result, "%s%s%s", cwd,
                         (cwd[0] && cwd[strlen(cwd) - 1] != '\\') ? "\\" : "",
                         nm);
                for (i = 0; i < n; i++)
                    free(names[i]);
                free(names);
                _cxl_werr(W_NOERROR);
                return result;
            }
        }
        for (i = 0; i < n; i++)
            free(names[i]);
        free(names);
        if (sel == -1)
            return NULL;
        if (sel == -2) {
            _cxl_werr(W_DOSERROR);
            return NULL;
        }
    }
}

/* In-place selection: the choice is shown at one spot and the arrow keys
 * or space step through the strings. */
int wselstr(int wrow, int wcol, int attr, char *strarr[], int initelem)
{
    int n = 0, maxlen = 0, sel, i;
    if (_cxl_wbeg())
        return -1;
    while (strarr[n]) {
        int l = strlen(strarr[n]);
        if (l > maxlen)
            maxlen = l;
        n++;
    }
    if (!n)
        return _cxl_werr(W_NOITMDEF), -1;
    sel = (initelem >= 0 && initelem < n) ? initelem : 0;
    for (;;) {
        unsigned key;
        int c;
        const char *s = strarr[sel];
        for (i = 0; i < maxlen; i++)
            wprintc(wrow, wcol + i, attr, *s ? (unsigned char)*s++ : ' ');
        wgotoxy(wrow, wcol);
        key = getxch();
        c = key & 0xff;
        if (c == 0x1b && _winfo.esc) {
            _cxl_werr(W_ESCPRESS);
            return -1;
        }
        if (c == '\r') {
            _cxl_werr(W_NOERROR);
            return sel;
        }
        if (c == ' ' || key == 0x4d00 || key == 0x5000)
            sel = (sel + 1) % n;
        else if (key == 0x4b00 || key == 0x4800 || key == 0x0e08)
            sel = (sel + n - 1) % n;
        else if (key == 0x4700)
            sel = 0;
        else if (key == 0x4f00)
            sel = n - 1;
        else if (c > ' ')
            for (i = 1; i <= n; i++) {
                int k = (sel + i) % n;
                if (toupper((unsigned char)strarr[k][0]) == toupper(c)) {
                    sel = k;
                    break;
                }
            }
    }
}

/* Attribute picker: foreground across, background down. */
int wpickattr(int srow, int scol, int btype, int initattr, int blink,
              void (*open)(void))
{
    int fg = initattr & 15, bg = (initattr >> 4) & 7, bl = blink ? initattr & 0x80 : 0;
    int erow = srow + 7 + 2 * (btype != 5), ecol = scol + 47 + 2 * (btype != 5);
    int r, c, result = -1, cs, ce;
    if (!wopen(srow, scol, erow, ecol, btype, 0x0f, 0x07))
        return -1;
    wtitle(" Attribute ", TCENTER, 0x0f);
    if (open)
        open();
    getcursz(&cs, &ce);
    hidecur();
    for (;;) {
        unsigned key;
        int k;
        char *txt;
        for (r = 0; r < 8; r++)
            for (c = 0; c < 16; c++) {
                int a = (r << 4) | c | bl;
                int on = r == bg && c == fg;
                wprintc(r, c * 3, a, on ? 0x10 : ' ');
                wprintc(r, c * 3 + 1, a, 'X');
                wprintc(r, c * 3 + 2, a, on ? 0x11 : ' ');
            }
        txt = attr2text((bg << 4) | fg | bl);
        if (W->border) {
            char msg[44];
            int len;
            snprintf(msg, sizeof msg, " %s ", txt);
            len = strlen(msg);
            for (k = 1; k < ecol - scol; k++)
                _cxl_vput(erow, scol + k, CELL(_cxl_boxch[btype][5], W->battr));
            wmessage(msg, BT_BORD, (ecol - scol + 1 - len) / 2, 0x0f);
        }
        key = getxch();
        k = key & 0xff;
        if (k == 0x1b && _winfo.esc) {
            _cxl_werr(W_ESCPRESS);
            break;
        }
        if (k == '\r') {
            result = (bg << 4) | fg | bl;
            _cxl_werr(W_NOERROR);
            break;
        }
        if ((k == 'b' || k == 'B') && blink)
            bl ^= 0x80;
        switch (key >> 8) {
        case 0x48: bg = (bg + 7) % 8; break;
        case 0x50: bg = (bg + 1) % 8; break;
        case 0x4b: fg = (fg + 15) % 16; break;
        case 0x4d: fg = (fg + 1) % 16; break;
        }
    }
    r = _winfo.errno;
    setcursz(cs, ce);
    wclose();
    _winfo.errno = r;
    return result;
}
