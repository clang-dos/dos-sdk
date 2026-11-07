/* Dialogs: templates, the dialog manager's keyboard interface, modal
 * loops and message boxes.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "cwin-local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char cw_dialogclass[] = "#32770";

static int
within(struct wnd *w, struct wnd *anc)
{
    for (; w; w = w->parent)
        if (w == anc)
            return 1;
    return 0;
}

static LRESULT
dlgcode(struct wnd *c, WPARAM wp, LPMSG m)
{
    return cw_send(c, WM_GETDLGCODE, wp, (LPARAM)(uintptr_t)m);
}

static int
usable(struct wnd *c)
{
    return (c->style & WS_VISIBLE) && !(c->style & WS_DISABLED);
}

/* ---- Tab and group order. ---- */

static int
anyTabStop(struct wnd *d)
{
    for (struct wnd *c = d->first; c; c = c->next)
        if (c->style & WS_TABSTOP)
            return 1;
    return 0;
}

static int
tabbable(struct wnd *d, struct wnd *c, int any)
{
    if (!usable(c))
        return 0;
    if (any)
        return (c->style & WS_TABSTOP) != 0;
    return !(dlgcode(c, 0, 0) & DLGC_STATIC);
}

HWND
GetNextDlgTabItem(HWND dlg, HWND ctl, BOOL prev)
{
    struct wnd *d = W(dlg), *c = W(ctl);
    if (!d || !d->first)
        return 0;
    if (c && c->parent != d)
        c = 0;
    int any = anyTabStop(d);
    struct wnd *start = c ? c : (prev ? d->first : d->last), *k = start;
    do {
        k = prev ? (k->prev ? k->prev : d->last) : (k->next ? k->next : d->first);
        if (tabbable(d, k, any))
            return k->hwnd;
    } while (k != start);
    return c && tabbable(d, c, any) ? c->hwnd : 0;
}

static struct wnd *
groupStart(struct wnd *c)
{
    while (c->prev && !(c->style & WS_GROUP))
        c = c->prev;
    return c;
}

HWND
GetNextDlgGroupItem(HWND dlg, HWND ctl, BOOL prev)
{
    struct wnd *d = W(dlg), *c = W(ctl);
    if (!d || !c || c->parent != d)
        return 0;
    struct wnd *first = groupStart(c), *last = first;
    while (last->next && !(last->next->style & WS_GROUP))
        last = last->next;
    struct wnd *k = c;
    do {
        if (prev)
            k = k == first ? last : k->prev;
        else
            k = k == last ? first : k->next;
        if (usable(k))
            return k->hwnd;
    } while (k != c);
    return ctl;
}

/* ---- Control helpers. ---- */

HWND
GetDlgItem(HWND dlg, int id)
{
    struct wnd *d = W(dlg);
    if (!d)
        return 0;
    for (struct wnd *c = d->first; c; c = c->next)
        if ((int)c->id == id)
            return c->hwnd;
    return 0;
}

int GetDlgCtrlID(HWND h) { struct wnd *w = W(h); return w ? (int)w->id : 0; }

LRESULT
SendDlgItemMessage(HWND dlg, int id, UINT msg, WPARAM wp, LPARAM lp)
{
    return SendMessage(GetDlgItem(dlg, id), msg, wp, lp);
}

void SetDlgItemText(HWND dlg, int id, LPCSTR t) { SetWindowText(GetDlgItem(dlg, id), t); }

int
GetDlgItemText(HWND dlg, int id, LPSTR buf, int max)
{
    if (max > 0)
        buf[0] = 0;
    return GetWindowText(GetDlgItem(dlg, id), buf, max);
}

void
SetDlgItemInt(HWND dlg, int id, UINT v, BOOL sign)
{
    char buf[8];
    if (sign)
        sprintf(buf, "%d", (int)v);
    else
        sprintf(buf, "%u", v);
    SetDlgItemText(dlg, id, buf);
}

UINT
GetDlgItemInt(HWND dlg, int id, BOOL *ok, BOOL sign)
{
    char buf[16];
    GetDlgItemText(dlg, id, buf, sizeof buf);
    char *p = buf;
    while (*p == ' ')
        p++;
    int neg = 0;
    if (sign && *p == '-') {
        neg = 1;
        p++;
    }
    long v = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p++ - '0');
        digits++;
        if (v > 65535L)
            break;
    }
    while (*p == ' ')
        p++;
    int good = digits && !*p && (sign ? v <= 32767L + neg : v <= 65535L);
    if (ok)
        *ok = good;
    if (!good)
        return 0;
    return (UINT)(neg ? -v : v);
}

void CheckDlgButton(HWND dlg, int id, UINT check) { SendDlgItemMessage(dlg, id, BM_SETCHECK, check, 0); }
UINT IsDlgButtonChecked(HWND dlg, int id) { return (UINT)SendDlgItemMessage(dlg, id, BM_GETCHECK, 0, 0); }

void
CheckRadioButton(HWND dlg, int first, int last, int check)
{
    for (int id = first; id <= last; id++)
        CheckDlgButton(dlg, id, id == check);
}

/* ---- The dialog window. ---- */

static void
focusCtl(struct wnd *c)
{
    if (!c)
        return;
    SetFocus(c->hwnd);
    if (dlgcode(c, 0, 0) & DLGC_HASSETSEL)
        cw_send(c, EM_SETSEL, 0, MAKELONG(0, -1));
}

static void
focusFirst(struct wnd *d)
{
    struct wnd *f = d->lastFocus;
    if (f && f != d && within(f, d) && usable(f)) {
        SetFocus(f->hwnd);
        return;
    }
    struct wnd *c = W(GetNextDlgTabItem(d->hwnd, 0, FALSE));
    if (c)
        focusCtl(c);
}

static LRESULT
dlgDefault(struct wnd *d, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND h = d->hwnd;
    switch (msg) {
    case WM_CLOSE: {
        HWND c = GetDlgItem(h, IDCANCEL);
        struct wnd *cw = W(c);
        if (!cw || !(cw->style & WS_DISABLED))
            cw_send(d, WM_COMMAND, IDCANCEL, MAKELONG(c, BN_CLICKED));
        return 0;
    }
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE && (!cw_focus || !within(cw_focus, d) || cw_focus == d))
            focusFirst(d);
        return 0;
    case WM_SETFOCUS:
        focusFirst(d);
        return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

LRESULT
DefDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    struct wnd *d = W(h);
    if (!d)
        return 0;
    if (d->dlgproc) {
        d->dlgresult = 0;
        if (d->dlgproc(h, msg, wp, lp)) {
            d = W(h);
            return d ? d->dlgresult : 0;
        }
        if (!W(h))
            return 0;
    }
    return dlgDefault(d, msg, wp, lp);
}

LRESULT cw_dialogproc(HWND h, UINT msg, WPARAM wp, LPARAM lp) { return DefDlgProc(h, msg, wp, lp); }

static struct wnd *
createDialog(HWND parent, const CWDLGTEMPLATE *t, DLGPROC proc, LPARAM param)
{
    cw_setup();
    DWORD style = (t->style | WS_POPUP) & ~(WS_VISIBLE | WS_CHILD);
    if (!(style & (WS_BORDER | WS_DLGFRAME | WS_THICKFRAME)))
        style |= WS_BORDER;
    struct wnd *pw = W(parent);
    int x = t->x, y = t->y;
    RECT dc;
    cw_clientrect(cw_desktop, &dc);
    if ((t->style & DS_CENTER) || x == CW_USEDEFAULT) {
        x = (dc.right - dc.left - t->cx) / 2;
        y = (dc.bottom - dc.top - t->cy) / 2;
    } else if (pw) {
        RECT pc;
        cw_clientrect(pw, &pc);
        x += pc.left - dc.left;
        y += pc.top - dc.top;
    }
    HWND h = CreateWindow(cw_dialogclass, t->caption, style, x, y, t->cx, t->cy, parent, 0, 0, 0);
    struct wnd *d = W(h);
    if (!d)
        return 0;
    d->isDialog = 1;
    d->dlgproc = proc;
    for (int i = 0; i < t->nitems; i++) {
        const CWDLGITEM *it = &t->items[i];
        CreateWindow(it->cls, it->text, it->style | WS_CHILD | WS_VISIBLE, it->x, it->y, it->cx,
                     it->cy, h, (HMENU)it->id, 0, 0);
    }
    HWND first = GetNextDlgTabItem(h, 0, FALSE);
    BOOL setFocus = TRUE;
    if (proc) {
        setFocus = proc(h, WM_INITDIALOG, first, param);
        if (!W(h))
            return 0;
    }
    if (setFocus) {
        struct wnd *f = W(first);
        if (f) {
            d->lastFocus = f;
            if (t->style & WS_VISIBLE)
                focusCtl(f);
        }
    } else if (cw_focus && within(cw_focus, d)) {
        d->lastFocus = cw_focus;
    }
    return d;
}

HWND
CwCreateDialog(HWND parent, const CWDLGTEMPLATE *t, DLGPROC proc, LPARAM param)
{
    struct wnd *d = createDialog(parent, t, proc, param);
    if (!d)
        return 0;
    if (t->style & WS_VISIBLE)
        ShowWindow(d->hwnd, SW_SHOW);
    return d->hwnd;
}

int
CwDialogBox(HWND parent, const CWDLGTEMPLATE *t, DLGPROC proc, LPARAM param)
{
    struct wnd *d = createDialog(parent, t, proc, param);
    if (!d)
        return -1;
    HWND h = d->hwnd;
    struct wnd *owner = d->owner;
    HWND oh = owner ? owner->hwnd : 0;
    int reenable = owner && !(owner->style & WS_DISABLED);
    if (reenable)
        EnableWindow(oh, FALSE);
    ShowWindow(h, SW_SHOW);
    if (!cw_focus || !within(cw_focus, d))
        focusFirst(d);
    int result = -1;
    MSG m;
    for (;;) {
        d = W(h);
        if (!d)
            break;
        if (d->ended) {
            result = d->endResult;
            break;
        }
        if (!GetMessage(&m, 0, 0, 0)) {
            PostQuitMessage((int)m.wParam);
            break;
        }
        if (!IsDialogMessage(h, &m)) {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
    }
    if (reenable && W(oh))
        EnableWindow(oh, TRUE);
    if (W(h))
        DestroyWindow(h);
    if (W(oh) && !(W(oh)->style & WS_DISABLED))
        SetActiveWindow(oh);
    return result;
}

BOOL
EndDialog(HWND h, int result)
{
    struct wnd *d = W(h);
    if (!d || !d->isDialog)
        return FALSE;
    d->ended = 1;
    d->endResult = result;
    return TRUE;
}

/* A control by its mnemonic; a label gives the focus to the control
 * after it. */
static int
mnemonic(struct wnd *d, int ch)
{
    int c = cw_upper(ch);
    for (struct wnd *k = d->first; k; k = k->next) {
        if (!usable(k) || cw_mnemonic(k->text) != c)
            continue;
        LRESULT code = dlgcode(k, 0, 0);
        if (code & DLGC_STATIC) {
            for (struct wnd *n = k->next; n; n = n->next)
                if (usable(n) && !(dlgcode(n, 0, 0) & DLGC_STATIC)) {
                    focusCtl(n);
                    break;
                }
            return 1;
        }
        focusCtl(k);
        if (code & DLGC_BUTTON)
            cw_send(k, WM_CHAR, ' ', 0);
        return 1;
    }
    return 0;
}

static struct wnd *
defButton(struct wnd *d)
{
    for (struct wnd *c = d->first; c; c = c->next)
        if (dlgcode(c, 0, 0) & DLGC_DEFPUSHBUTTON)
            return c;
    return W(GetDlgItem(d->hwnd, IDOK));
}

BOOL
IsDialogMessage(HWND dlg, LPMSG m)
{
    struct wnd *d = W(dlg), *t = W(m->hwnd);
    if (!d || !t || !within(t, d))
        return FALSE;
    switch (m->message) {
    case WM_KEYDOWN: {
        LRESULT code = dlgcode(t, m->wParam, m);
        int vk = (int)m->wParam;
        int shift = GetKeyState(VK_SHIFT) < 0;
        if (vk == VK_TAB && !(code & (DLGC_WANTTAB | DLGC_WANTALLKEYS))) {
            focusCtl(W(GetNextDlgTabItem(dlg, cw_focus ? cw_focus->hwnd : 0, shift)));
            return TRUE;
        }
        if ((vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN) &&
            !(code & (DLGC_WANTARROWS | DLGC_WANTALLKEYS)) && cw_focus && cw_focus->parent == d) {
            struct wnd *n = W(GetNextDlgGroupItem(dlg, cw_focus->hwnd, vk == VK_LEFT || vk == VK_UP));
            if (n && n != cw_focus) {
                focusCtl(n);
                if (dlgcode(n, 0, 0) & DLGC_RADIOBUTTON)
                    cw_send(n, WM_CHAR, ' ', 0);
            }
            return TRUE;
        }
        if (vk == VK_RETURN && !(code & DLGC_WANTALLKEYS)) {
            if ((code & (DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON)) && t == cw_focus) {
                cw_send(t, WM_CHAR, ' ', 0);
                return TRUE;
            }
            struct wnd *b = defButton(d);
            if (b && (b->style & WS_DISABLED))
                MessageBeep(0);
            else
                cw_send(d, WM_COMMAND, b ? b->id : IDOK, MAKELONG(b ? b->hwnd : 0, BN_CLICKED));
            return TRUE;
        }
        if (vk == VK_ESCAPE) {
            HWND c = GetDlgItem(dlg, IDCANCEL);
            struct wnd *cw = W(c);
            if (cw && (cw->style & WS_DISABLED))
                MessageBeep(0);
            else
                cw_send(d, WM_COMMAND, IDCANCEL, MAKELONG(c, BN_CLICKED));
            return TRUE;
        }
        break;
    }
    case WM_CHAR:
        if (m->wParam > ' ' && !(dlgcode(t, m->wParam, m) & DLGC_WANTCHARS) && mnemonic(d, (int)m->wParam))
            return TRUE;
        break;
    case WM_SYSCHAR:
        if (mnemonic(d, (int)m->wParam))
            return TRUE;
        break;
    }
    TranslateMessage(m);
    DispatchMessage(m);
    return TRUE;
}

/* ---- Message boxes. ---- */

struct mbinfo {
    UINT type;
    int ids[3], n, def;
};

static BOOL
mbProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    struct mbinfo *mb = (struct mbinfo *)(uintptr_t)GetWindowLong(h, DWL_USER);
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLong(h, DWL_USER, lp);
        mb = (struct mbinfo *)(uintptr_t)lp;
        SetFocus(GetDlgItem(h, mb->ids[mb->def]));
        return FALSE;
    case WM_COMMAND:
        for (int i = 0; i < mb->n; i++)
            if ((int)wp == mb->ids[i]) {
                EndDialog(h, (int)wp);
                return TRUE;
            }
        if (wp == IDCANCEL && mb->n == 1)
            EndDialog(h, mb->ids[0]);
        return TRUE;
    }
    return FALSE;
}

int
MessageBox(HWND parent, LPCSTR text, LPCSTR caption, UINT type)
{
    cw_setup();
    static const char *const names[] = { 0, "OK", "Cancel", "&Abort", "&Retry", "&Ignore", "&Yes", "&No" };
    static const BYTE sets[6][3] = {
        { IDOK }, { IDOK, IDCANCEL }, { IDABORT, IDRETRY, IDIGNORE },
        { IDYES, IDNO, IDCANCEL }, { IDYES, IDNO }, { IDRETRY, IDCANCEL },
    };
    struct mbinfo mb;
    memset(&mb, 0, sizeof mb);
    mb.type = type;
    UINT set = type & 0x0f;
    if (set > 5)
        set = 0;
    for (int i = 0; i < 3 && sets[set][i]; i++)
        mb.ids[mb.n++] = sets[set][i];
    mb.def = (int)((type >> 8) & 3);
    if (mb.def >= mb.n)
        mb.def = 0;
    if (!text)
        text = "";
    if (!caption)
        caption = "Error";
    UINT icon = type & 0xf0;
    if (icon == MB_ICONHAND || icon == MB_ICONEXCLAMATION)
        MessageBeep(type);

    /* Measure the text, wrapped to at most 60 columns. */
    RECT dr;
    cw_clientrect(cw_desktop, &dr);
    int maxw = cw_imin(60, dr.right - dr.left - 10), longest = 0, lines = 0;
    for (const char *p = text;;) {
        const char *e = strchr(p, '\n');
        int n = e ? (int)(e - p) : (int)strlen(p);
        if (n > 0 && p[n - 1] == '\r')
            n--;
        longest = cw_imax(longest, cw_imin(n, maxw));
        lines += n > maxw ? (n + maxw - 1) / maxw + 1 : 1;
        if (!e)
            break;
        p = e + 1;
    }
    int bw[3], total = 0;
    for (int i = 0; i < mb.n; i++) {
        bw[i] = cw_imax(10, cw_textlen(names[mb.ids[i]]) + 4);
        total += bw[i] + (i ? 2 : 0);
    }
    int inner = cw_imax(cw_imax(longest, total), cw_textlen(caption) + 2);
    int w = inner + 6;
    lines = cw_imin(lines, dr.bottom - dr.top - 8);
    int h = lines + 6;
    CWDLGITEM items[4];
    items[0].cls = "STATIC";
    items[0].text = text;
    items[0].id = 0xffff;
    items[0].style = longest < maxw && lines == 1 ? SS_CENTER | SS_NOPREFIX : SS_LEFT | SS_NOPREFIX;
    items[0].x = 2;
    items[0].y = 1;
    items[0].cx = inner;
    items[0].cy = lines;
    int x = 1 + (w - 2 - total) / 2;
    for (int i = 0; i < mb.n; i++) {
        CWDLGITEM *it = &items[1 + i];
        it->cls = "BUTTON";
        it->text = names[mb.ids[i]];
        it->id = (UINT)mb.ids[i];
        it->style = WS_TABSTOP | (i == mb.def ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON) | (i == 0 ? WS_GROUP : 0);
        it->x = x - 1;
        it->y = lines + 2;
        it->cx = bw[i];
        it->cy = 1;
        x += bw[i] + 2;
    }
    CWDLGTEMPLATE t = { WS_POPUP | WS_CAPTION | DS_CENTER, 0, 0, w, h, caption, 1 + mb.n, items };
    if (!parent && cw_active)
        parent = cw_active->hwnd;
    return CwDialogBox(parent, &t, mbProc, (LPARAM)(uintptr_t)&mb);
}
