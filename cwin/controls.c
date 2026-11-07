/* The predefined control classes: BUTTON, STATIC, EDIT and LISTBOX.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "cwin-local.h"
#include <stdlib.h>
#include <string.h>

#define PTR(lp) ((void *)(uintptr_t)(lp))

static void
notify(struct wnd *w, UINT code)
{
    if (w->parent)
        cw_send(w->parent, WM_COMMAND, w->id, MAKELONG(w->hwnd, code));
}

static int
focused(struct wnd *w)
{
    return cw_focus == w;
}

/* The caret on a control while it has the focus. */
static void
caretOn(struct wnd *w, int x, int y)
{
    CreateCaret(w->hwnd, 0, 1, 1);
    SetCaretPos(x, y);
    ShowCaret(w->hwnd);
}

/* ---- BUTTON ---- */

struct button {
    BYTE state;  /* BST_ */
    BYTE pushed; /* the mouse is down on it */
};

static int btype(struct wnd *w) { return (int)(w->style & 0x0f); }

static int
isPush(struct wnd *w)
{
    int t = btype(w);
    return t == BS_PUSHBUTTON || t == BS_DEFPUSHBUTTON;
}

/* A push button looks default when it has the focus, or is the default
 * one and no other push button has the focus. */
static int
looksDefault(struct wnd *w)
{
    if (focused(w))
        return 1;
    if (btype(w) != BS_DEFPUSHBUTTON)
        return 0;
    struct wnd *f = cw_focus;
    return !(f && f->parent == w->parent && f->cls == w->cls && isPush(f));
}

static void
buttonClick(struct wnd *w)
{
    struct button *b = (struct button *)w->ctl;
    HWND h = w->hwnd;
    switch (btype(w)) {
    case BS_AUTOCHECKBOX:
        b->state = !b->state;
        break;
    case BS_AUTO3STATE:
        b->state = (BYTE)((b->state + 1) % 3);
        break;
    case BS_AUTORADIOBUTTON:
        /* Check this one, uncheck the others of its group. */
        if (w->parent) {
            HWND first = GetNextDlgGroupItem(w->parent->hwnd, h, TRUE);
            HWND c = first ? first : h;
            for (int i = 0; i < 64; i++) {
                struct wnd *k = W(c);
                if (k && k->cls == w->cls && btype(k) == BS_AUTORADIOBUTTON)
                    ((struct button *)k->ctl)->state = k == w;
                c = GetNextDlgGroupItem(w->parent->hwnd, c, FALSE);
                if (!c || c == (first ? first : h))
                    break;
            }
        }
        b->state = 1;
        break;
    }
    cw_dirty = 1;
    if (W(h))
        notify(w, BN_CLICKED);
}

static void
buttonPaint(struct wnd *w)
{
    struct button *b = (struct button *)w->ctl;
    HDC dc = BeginPaint(w->hwnd, 0);
    BYTE a = cw_ctlattr(w), hot = (BYTE)((a & 0xf0) | 0x0f);
    int disabled = (w->style & WS_DISABLED) != 0;
    if (disabled)
        a = hot = (BYTE)((a & 0xf0) | 0x08);
    RECT r;
    cw_rect(&r, 0, 0, w->w, w->h);
    int y = (w->h - 1) / 2;
    int t = btype(w);
    if (t == BS_GROUPBOX) {
        CwDrawBox(dc, &r, a, FALSE);
        if (w->text[0]) {
            CwSetTextAttr(dc, a);
            RECT tr;
            cw_rect(&tr, 2, 0, w->w - 4, 1);
            int n = cw_imin(cw_textlen(w->text), w->w - 4);
            CwPutChar(dc, 1, 0, ' ', a);
            DrawText(dc, w->text, -1, &tr, DT_SINGLELINE | DT_LEFT);
            CwPutChar(dc, 2 + n, 0, ' ', a);
        }
        EndPaint(w->hwnd, 0);
        return;
    }
    CwFillRect(dc, &r, ' ', a);
    if (isPush(w)) {
        BYTE ta = a;
        if (b->pushed)
            ta = cw_sys[COLOR_HIGHLIGHT];
        else if (focused(w))
            ta = cw_sys[COLOR_BTNFOCUS];
        BYTE ba = looksDefault(w) && !disabled ? (BYTE)((a & 0xf0) | 0x0f) : a;
        if (b->pushed)
            ba = ta;
        CwPutChar(dc, 0, y, '<', ba);
        CwPutChar(dc, w->w - 1, y, '>', ba);
        RECT tr;
        cw_rect(&tr, 1, y, w->w - 2, 1);
        CwSetTextAttr(dc, ta);
        if (b->pushed || focused(w))
            CwFillRect(dc, &tr, ' ', ta);
        DrawText(dc, w->text, -1, &tr, DT_SINGLELINE | DT_CENTER);
        if (focused(w)) {
            int n = cw_textlen(w->text);
            caretOn(w, 1 + (w->w - 2 - n) / 2, y);
        }
    } else {
        int radio = t == BS_RADIOBUTTON || t == BS_AUTORADIOBUTTON;
        int left = (w->style & BS_LEFTTEXT) != 0;
        int bx = left ? w->w - 3 : 0;
        char mark = (char)(b->state == 1 ? (radio ? 0x07 : 'X') : b->state == 2 ? '?' : ' ');
        CwPutChar(dc, bx, y, radio ? '(' : '[', a);
        CwPutChar(dc, bx + 1, y, (BYTE)mark, a);
        CwPutChar(dc, bx + 2, y, radio ? ')' : ']', a);
        RECT tr;
        cw_rect(&tr, left ? 0 : 4, y, w->w - 4, 1);
        CwSetTextAttr(dc, a);
        DrawText(dc, w->text, -1, &tr, DT_SINGLELINE | (left ? DT_RIGHT : DT_LEFT));
        if (focused(w))
            caretOn(w, bx + 1, y);
    }
    EndPaint(w->hwnd, 0);
}

static LRESULT
buttonProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    struct wnd *w = W(h);
    struct button *b = (struct button *)w->ctl;
    switch (msg) {
    case WM_CREATE:
        w->ctl = calloc(1, sizeof(struct button));
        return w->ctl ? 0 : -1;
    case WM_DESTROY:
        free(w->ctl);
        w->ctl = 0;
        return 0;
    case WM_PAINT:
        buttonPaint(w);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_GETDLGCODE: {
        int t = btype(w);
        if (t == BS_GROUPBOX)
            return DLGC_STATIC;
        if (t == BS_DEFPUSHBUTTON)
            return DLGC_BUTTON | DLGC_DEFPUSHBUTTON;
        if (t == BS_PUSHBUTTON)
            return DLGC_BUTTON | DLGC_UNDEFPUSHBUTTON;
        if (t == BS_RADIOBUTTON || t == BS_AUTORADIOBUTTON)
            return DLGC_BUTTON | DLGC_RADIOBUTTON;
        return DLGC_BUTTON;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        if (msg == WM_KILLFOCUS)
            DestroyCaret();
        cw_dirty = 1;
        return 0;
    case WM_CHAR:
        if (wp == ' ' && btype(w) != BS_GROUPBOX)
            buttonClick(w);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        if (btype(w) == BS_GROUPBOX)
            return 0;
        SetFocus(h);
        SetCapture(h);
        b->pushed = 1;
        cw_dirty = 1;
        return 0;
    case WM_MOUSEMOVE:
        if (GetCapture() == h) {
            int in = (int)(short)LOWORD(lp) >= 0 && (int)(short)LOWORD(lp) < w->w &&
                     (int)(short)HIWORD(lp) >= 0 && (int)(short)HIWORD(lp) < w->h;
            if (b->pushed != in) {
                b->pushed = (BYTE)in;
                cw_dirty = 1;
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (GetCapture() == h) {
            ReleaseCapture();
            int was = b->pushed;
            b->pushed = 0;
            cw_dirty = 1;
            if (was)
                buttonClick(w);
        }
        return 0;
    case BM_GETCHECK:
        return b->state;
    case BM_SETCHECK:
        b->state = (BYTE)wp;
        cw_dirty = 1;
        return 0;
    case BM_GETSTATE:
        return b->state | (b->pushed ? 4 : 0) | (focused(w) ? 8 : 0);
    case BM_SETSTATE:
        b->pushed = wp != 0;
        cw_dirty = 1;
        return 0;
    case BM_SETSTYLE:
        w->style = (w->style & ~0xffL) | (wp & 0xff);
        cw_dirty = 1;
        return 0;
    case WM_ENABLE:
        cw_dirty = 1;
        return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

/* ---- STATIC ---- */

static LRESULT
staticProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    struct wnd *w = W(h);
    switch (msg) {
    case WM_PAINT: {
        HDC dc = BeginPaint(h, 0);
        BYTE a = cw_ctlattr(w);
        if (w->style & WS_DISABLED)
            a = (BYTE)((a & 0xf0) | 0x08);
        RECT r;
        cw_rect(&r, 0, 0, w->w, w->h);
        CwFillRect(dc, &r, ' ', a);
        CwSetTextAttr(dc, a);
        UINT s = (UINT)(w->style & 0x0f), fmt = DT_WORDBREAK;
        if (s == SS_CENTER)
            fmt |= DT_CENTER;
        else if (s == SS_RIGHT)
            fmt |= DT_RIGHT;
        else if (s == SS_SIMPLE)
            fmt = DT_SINGLELINE;
        if (w->style & SS_NOPREFIX)
            fmt |= DT_NOPREFIX;
        DrawText(dc, w->text, -1, &r, fmt);
        EndPaint(h, 0);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_GETDLGCODE:
        return DLGC_STATIC;
    }
    return DefWindowProc(h, msg, wp, lp);
}

/* ---- EDIT ---- */

struct edit {
    char *buf;
    int len, cap, limit;
    int caret, anchor; /* the selection is between them */
    int top, left;     /* the first line and column shown */
    BYTE modified, selecting;
};

static int multi(struct wnd *w) { return (w->style & ES_MULTILINE) != 0; }

static int
viewW(struct wnd *w)
{
    RECT r;
    GetClientRect(w->hwnd, &r);
    return cw_imax(1, r.right);
}

static int
viewH(struct wnd *w)
{
    RECT r;
    GetClientRect(w->hwnd, &r);
    return cw_imax(1, r.bottom);
}

static int
lineStart(struct edit *e, int pos)
{
    while (pos > 0 && e->buf[pos - 1] != '\n')
        pos--;
    return pos;
}

static int
lineEnd(struct edit *e, int pos)
{
    while (pos < e->len && e->buf[pos] != '\n')
        pos++;
    return pos;
}

static int
lineOf(struct edit *e, int pos)
{
    int n = 0;
    for (int i = 0; i < pos && i < e->len; i++)
        if (e->buf[i] == '\n')
            n++;
    return n;
}

static int
lineIndex(struct edit *e, int line)
{
    int pos = 0;
    while (line > 0 && pos <= e->len) {
        int end = lineEnd(e, pos);
        if (end >= e->len)
            return -1;
        pos = end + 1;
        line--;
    }
    return pos;
}

static int
lineCount(struct edit *e)
{
    return lineOf(e, e->len) + 1;
}

static void
editScrollbar(struct wnd *w, struct edit *e)
{
    if (!(w->style & WS_VSCROLL))
        return;
    int n = lineCount(e) - 1;
    w->sbmin = 0;
    w->sbmax = n > 0 ? n : 0;
    w->sbpos = cw_imin(e->top, w->sbmax);
}

/* Scroll so that the caret shows, and put the caret there. */
static void
editShow(struct wnd *w, struct edit *e)
{
    int ls = lineStart(e, e->caret), col = e->caret - ls, line = lineOf(e, e->caret);
    int vw = viewW(w), vh = viewH(w);
    if (col < e->left)
        e->left = col;
    if (col >= e->left + vw)
        e->left = col - vw + 1;
    if (multi(w)) {
        if (line < e->top)
            e->top = line;
        if (line >= e->top + vh)
            e->top = line - vh + 1;
    }
    editScrollbar(w, e);
    if (focused(w))
        SetCaretPos(col - e->left, multi(w) ? line - e->top : 0);
    cw_dirty = 1;
}

static int
selMin(struct edit *e)
{
    return cw_imin(e->caret, e->anchor);
}

static int
selMax(struct edit *e)
{
    return cw_imax(e->caret, e->anchor);
}

static void
editDelete(struct edit *e, int a, int b)
{
    memmove(e->buf + a, e->buf + b, e->len - b + 1);
    e->len -= b - a;
    e->caret = e->anchor = a;
}

/* Replace the selection with s (n characters); FALSE when it is full. */
static int
editInsert(struct wnd *w, struct edit *e, const char *s, int n)
{
    if (selMax(e) > selMin(e))
        editDelete(e, selMin(e), selMax(e));
    int room = e->limit - e->len;
    int full = 0;
    if (n > room) {
        n = room;
        full = 1;
    }
    if (n <= 0) {
        if (full)
            notify(w, EN_MAXTEXT);
        return !full;
    }
    if (e->len + n + 1 > e->cap) {
        int c = e->cap;
        while (c < e->len + n + 1)
            c = c * 2;
        char *t = (char *)realloc(e->buf, c);
        if (!t)
            return FALSE;
        e->buf = t;
        e->cap = c;
    }
    memmove(e->buf + e->caret + n, e->buf + e->caret, e->len - e->caret + 1);
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (w->style & ES_UPPERCASE)
            c = (char)cw_upper((unsigned char)c);
        else if ((w->style & ES_LOWERCASE) && c >= 'A' && c <= 'Z')
            c = (char)(c + 32);
        e->buf[e->caret + i] = c;
    }
    e->len += n;
    e->caret += n;
    e->anchor = e->caret;
    if (full)
        notify(w, EN_MAXTEXT);
    return TRUE;
}

static void
editChanged(struct wnd *w, struct edit *e)
{
    e->modified = 1;
    editShow(w, e);
    notify(w, EN_CHANGE);
}

static int
editSetText(struct wnd *w, struct edit *e, const char *s)
{
    e->len = 0;
    e->buf[0] = 0;
    e->caret = e->anchor = e->top = e->left = 0;
    if (s) {
        /* Lines end in \n; a \r is dropped. Single-line controls stop at the
         * first line end. */
        for (; *s; s++) {
            if (*s == '\r')
                continue;
            if (*s == '\n' && !multi(w))
                break;
            if (!editInsert(w, e, s, 1))
                break;
        }
    }
    e->caret = e->anchor = 0;
    e->modified = 0;
    editShow(w, e);
    return TRUE;
}

static void
editPaint(struct wnd *w, struct edit *e)
{
    HDC dc = BeginPaint(w->hwnd, 0);
    BYTE a = w->parent && cw_isdlg(w->parent) ? cw_sys[COLOR_EDIT] : cw_ctlattr(w);
    if (w->style & WS_DISABLED)
        a = (BYTE)((a & 0xf0) | 0x08);
    BYTE sa = cw_sys[COLOR_HIGHLIGHT];
    int vw = viewW(w), vh = viewH(w);
    int showSel = focused(w) || (w->style & ES_NOHIDESEL);
    int s0 = selMin(e), s1 = selMax(e);
    int pos = multi(w) ? lineIndex(e, e->top) : 0;
    for (int row = 0; row < vh; row++) {
        for (int x = 0; x < vw; x++)
            CwPutChar(dc, x, row, ' ', a);
        if (pos < 0 || (row > 0 && !multi(w)))
            continue;
        int end = lineEnd(e, pos);
        for (int col = e->left; col < e->left + vw && pos + col < end; col++) {
            int i = pos + col;
            BYTE ch = (BYTE)e->buf[i];
            if (w->style & ES_PASSWORD)
                ch = '*';
            CwPutChar(dc, col - e->left, row, ch, showSel && i >= s0 && i < s1 ? sa : a);
        }
        /* A selected line end shows as one selected cell. */
        if (showSel && end < e->len && end >= s0 && end < s1 && end - pos - e->left < vw &&
            end - pos >= e->left)
            CwPutChar(dc, end - pos - e->left, row, ' ', sa);
        pos = end < e->len ? end + 1 : -1;
    }
    EndPaint(w->hwnd, 0);
}

/* Move the caret to pos (and the anchor too, without Shift). */
static void
editMove(struct wnd *w, struct edit *e, int pos, int extend)
{
    if (pos < 0)
        pos = 0;
    if (pos > e->len)
        pos = e->len;
    e->caret = pos;
    if (!extend)
        e->anchor = pos;
    editShow(w, e);
}

static int
editPosAt(struct wnd *w, struct edit *e, int x, int y)
{
    int line = multi(w) ? e->top + cw_imax(0, y) : 0;
    int ls = lineIndex(e, line);
    if (ls < 0)
        return e->len;
    int le = lineEnd(e, ls);
    return cw_imin(le, ls + e->left + cw_imax(0, x));
}

static void
editVert(struct wnd *w, struct edit *e, int lines, int extend)
{
    int ls = lineStart(e, e->caret), col = e->caret - ls;
    int line = cw_imax(0, cw_imin(lineCount(e) - 1, lineOf(e, e->caret) + lines));
    int s = lineIndex(e, line);
    editMove(w, e, cw_imin(lineEnd(e, s), s + col), extend);
}

static void
editKey(struct wnd *w, struct edit *e, int vk)
{
    int shift = GetKeyState(VK_SHIFT) < 0, ctrl = GetKeyState(VK_CONTROL) < 0;
    int ro = (w->style & ES_READONLY) != 0;
    switch (vk) {
    case VK_LEFT:
        if (ctrl) {
            int p = e->caret;
            while (p > 0 && e->buf[p - 1] == ' ')
                p--;
            while (p > 0 && e->buf[p - 1] != ' ' && e->buf[p - 1] != '\n')
                p--;
            editMove(w, e, p, shift);
        } else if (!shift && e->anchor != e->caret)
            editMove(w, e, selMin(e), 0);
        else
            editMove(w, e, e->caret - 1, shift);
        break;
    case VK_RIGHT:
        if (ctrl) {
            int p = e->caret;
            while (p < e->len && e->buf[p] != ' ' && e->buf[p] != '\n')
                p++;
            while (p < e->len && e->buf[p] == ' ')
                p++;
            editMove(w, e, p, shift);
        } else if (!shift && e->anchor != e->caret)
            editMove(w, e, selMax(e), 0);
        else
            editMove(w, e, e->caret + 1, shift);
        break;
    case VK_HOME:
        editMove(w, e, ctrl ? 0 : lineStart(e, e->caret), shift);
        break;
    case VK_END:
        editMove(w, e, ctrl ? e->len : lineEnd(e, e->caret), shift);
        break;
    case VK_UP:
    case VK_DOWN:
        if (multi(w))
            editVert(w, e, vk == VK_UP ? -1 : 1, shift);
        break;
    case VK_PRIOR:
    case VK_NEXT:
        if (multi(w)) {
            int n = cw_imax(1, viewH(w) - 1);
            e->top = cw_imax(0, e->top + (vk == VK_PRIOR ? -n : n));
            e->top = cw_imin(e->top, cw_imax(0, lineCount(e) - 1));
            editVert(w, e, vk == VK_PRIOR ? -n : n, shift);
        }
        break;
    case VK_DELETE:
        if (ro)
            break;
        if (e->anchor != e->caret)
            editDelete(e, selMin(e), selMax(e));
        else if (e->caret < e->len)
            editDelete(e, e->caret, e->caret + 1);
        else
            break;
        editChanged(w, e);
        break;
    }
}

static void
editChar(struct wnd *w, struct edit *e, int ch)
{
    if (w->style & ES_READONLY)
        return;
    if (ch == 8) {
        if (e->anchor != e->caret)
            editDelete(e, selMin(e), selMax(e));
        else if (e->caret > 0)
            editDelete(e, e->caret - 1, e->caret);
        else
            return;
        editChanged(w, e);
        return;
    }
    if (ch == 1) { /* Ctrl+A: select all */
        e->anchor = 0;
        e->caret = e->len;
        editShow(w, e);
        return;
    }
    char c = (char)ch;
    if (ch == '\r') {
        if (!multi(w))
            return;
        c = '\n';
    } else if ((BYTE)ch < ' ' && ch != '\t') {
        return;
    } else if (ch == '\t' && !multi(w)) {
        return;
    }
    if (editInsert(w, e, &c, 1))
        editChanged(w, e);
    else
        MessageBeep(0);
}

static LRESULT
editProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    struct wnd *w = W(h);
    struct edit *e = (struct edit *)w->ctl;
    switch (msg) {
    case WM_CREATE: {
        e = (struct edit *)calloc(1, sizeof *e);
        if (!e)
            return -1;
        e->cap = 32;
        e->buf = (char *)malloc(e->cap);
        if (!e->buf) {
            free(e);
            return -1;
        }
        e->buf[0] = 0;
        e->limit = multi(w) ? 30000 : 255;
        w->ctl = e;
        editSetText(w, e, w->text);
        return 0;
    }
    case WM_DESTROY:
        if (e) {
            free(e->buf);
            free(e);
            w->ctl = 0;
        }
        return 0;
    case WM_PAINT:
        editPaint(w, e);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        editShow(w, e);
        return 0;
    case WM_GETDLGCODE:
        if (multi(w) && (w->style & ES_WANTRETURN))
            return DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS | DLGC_WANTALLKEYS;
        return DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS;
    case WM_SETFOCUS:
        CreateCaret(h, 0, 1, 1);
        ShowCaret(h);
        editShow(w, e);
        notify(w, EN_SETFOCUS);
        return 0;
    case WM_KILLFOCUS:
        DestroyCaret();
        cw_dirty = 1;
        notify(w, EN_KILLFOCUS);
        return 0;
    case WM_KEYDOWN:
        editKey(w, e, (int)wp);
        return 0;
    case WM_CHAR:
        editChar(w, e, (int)wp);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetFocus(h);
        editMove(w, e, editPosAt(w, e, (short)LOWORD(lp), (short)HIWORD(lp)),
                 (wp & MK_SHIFT) != 0);
        SetCapture(h);
        e->selecting = 1;
        return 0;
    case WM_MOUSEMOVE:
        if (e->selecting)
            editMove(w, e, editPosAt(w, e, (short)LOWORD(lp), (short)HIWORD(lp)), 1);
        return 0;
    case WM_LBUTTONUP:
        if (e->selecting) {
            e->selecting = 0;
            ReleaseCapture();
        }
        return 0;
    case WM_VSCROLL: {
        int n = lineCount(e), vh = viewH(w), top = e->top;
        switch (wp) {
        case SB_LINEUP: top--; break;
        case SB_LINEDOWN: top++; break;
        case SB_PAGEUP: top -= vh; break;
        case SB_PAGEDOWN: top += vh; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: top = (short)LOWORD(lp); break;
        case SB_TOP: top = 0; break;
        case SB_BOTTOM: top = n; break;
        }
        e->top = cw_imax(0, cw_imin(top, n - 1));
        editScrollbar(w, e);
        /* Keep the caret in view. */
        int line = lineOf(e, e->caret);
        if (line < e->top)
            editVert(w, e, e->top - line, 0);
        else if (line >= e->top + vh)
            editVert(w, e, e->top + vh - 1 - line, 0);
        cw_dirty = 1;
        return 0;
    }
    case WM_SETTEXT:
        DefWindowProc(h, msg, wp, lp);
        return editSetText(w, e, (const char *)PTR(lp));
    case WM_GETTEXT: {
        char *buf = (char *)PTR(lp);
        int max = (int)wp;
        if (max <= 0 || !buf)
            return 0;
        int n = cw_imin(e->len, max - 1);
        memcpy(buf, e->buf, n);
        buf[n] = 0;
        return n;
    }
    case WM_GETTEXTLENGTH:
        return e->len;
    case EM_GETSEL:
        return MAKELONG(selMin(e), selMax(e));
    case EM_SETSEL: {
        int a = (short)LOWORD(lp), b = (short)HIWORD(lp);
        if (a < 0 || a > e->len)
            a = e->len;
        if (b < 0 || b > e->len)
            b = e->len;
        e->anchor = a;
        e->caret = b;
        editShow(w, e);
        return 0;
    }
    case EM_REPLACESEL: {
        const char *s = (const char *)PTR(lp);
        for (; s && *s; s++)
            if (*s != '\r' && (*s != '\n' || multi(w)))
                if (!editInsert(w, e, s, 1))
                    break;
        editChanged(w, e);
        return 0;
    }
    case EM_GETMODIFY:
        return e->modified;
    case EM_SETMODIFY:
        e->modified = wp != 0;
        return 0;
    case EM_GETLINECOUNT:
        return lineCount(e);
    case EM_LINEINDEX:
        return lineIndex(e, (short)wp < 0 ? lineOf(e, e->caret) : (int)wp);
    case EM_LINEFROMCHAR:
        return lineOf(e, (short)wp < 0 ? e->caret : (int)wp);
    case EM_LINELENGTH: {
        int p = (short)wp < 0 ? e->caret : (int)wp;
        if (p > e->len)
            p = e->len;
        int s = lineStart(e, p);
        return lineEnd(e, s) - s;
    }
    case EM_GETLINE: {
        char *buf = (char *)PTR(lp);
        int s = lineIndex(e, (int)wp);
        if (!buf || s < 0)
            return 0;
        WORD max;
        memcpy(&max, buf, 2);
        int n = cw_imin(lineEnd(e, s) - s, max);
        memcpy(buf, e->buf + s, n);
        return n;
    }
    case EM_LIMITTEXT:
        e->limit = wp ? (int)wp : (multi(w) ? 30000 : 32766);
        return 0;
    case EM_SETREADONLY:
        if (wp)
            w->style |= ES_READONLY;
        else
            w->style &= ~ES_READONLY;
        return TRUE;
    case EM_LINESCROLL:
        e->top = cw_imax(0, cw_imin(e->top + (short)LOWORD(lp), lineCount(e) - 1));
        editScrollbar(w, e);
        cw_dirty = 1;
        return TRUE;
    case EM_GETRECT: {
        RECT *r = (RECT *)PTR(lp);
        if (r)
            GetClientRect(h, r);
        return 0;
    }
    case WM_ENABLE:
        cw_dirty = 1;
        return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

/* ---- LISTBOX ---- */

struct list {
    char **items;
    int n, cap, sel, top;
};

static void
listShow(struct wnd *w, struct list *l)
{
    int vh = viewH(w);
    if (l->sel >= 0) {
        if (l->sel < l->top)
            l->top = l->sel;
        if (l->sel >= l->top + vh)
            l->top = l->sel - vh + 1;
    }
    l->top = cw_imax(0, cw_imin(l->top, l->n - vh));
    if (w->style & WS_VSCROLL) {
        w->sbmin = 0;
        w->sbmax = cw_imax(0, l->n - vh);
        w->sbpos = l->top;
    }
    cw_dirty = 1;
}

static int
lcmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = cw_upper((unsigned char)*a), y = cw_upper((unsigned char)*b);
        if (x != y || !x)
            return x - y;
    }
}

static int
listInsert(struct wnd *w, struct list *l, int at, const char *s)
{
    if (l->n == l->cap) {
        int c = l->cap ? l->cap * 2 : 16;
        char **t = (char **)realloc(l->items, c * sizeof *t);
        if (!t)
            return LB_ERRSPACE;
        l->items = t;
        l->cap = c;
    }
    char *d = cw_strdup(s);
    if (!d)
        return LB_ERRSPACE;
    if (at < 0 || at > l->n)
        at = l->n;
    memmove(l->items + at + 1, l->items + at, (l->n - at) * sizeof *l->items);
    l->items[at] = d;
    l->n++;
    if (l->sel >= at)
        l->sel++;
    listShow(w, l);
    return at;
}

/* The first item from start (after it, wrapping) that begins with s
 * (exact: equals s), ignoring case, or LB_ERR. */
static int
listFind(struct list *l, int start, const char *s, int exact)
{
    int n = (int)strlen(s);
    for (int k = 1; k <= l->n; k++) {
        int i = (start + k + l->n) % l->n;
        const char *t = l->items[i];
        int j = 0;
        while (j < n && t[j] && cw_upper((unsigned char)t[j]) == cw_upper((unsigned char)s[j]))
            j++;
        if (j == n && (!exact || !t[j]))
            return i;
    }
    return LB_ERR;
}

static void
listSelect(struct wnd *w, struct list *l, int i, int user)
{
    if (l->n == 0)
        return;
    i = cw_imax(0, cw_imin(i, l->n - 1));
    int changed = i != l->sel;
    l->sel = i;
    listShow(w, l);
    if (user && changed && (w->style & LBS_NOTIFY))
        notify(w, LBN_SELCHANGE);
}

static LRESULT
listProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    struct wnd *w = W(h);
    struct list *l = (struct list *)w->ctl;
    switch (msg) {
    case WM_CREATE:
        l = (struct list *)calloc(1, sizeof *l);
        if (!l)
            return -1;
        l->sel = -1;
        w->ctl = l;
        listShow(w, l);
        return 0;
    case WM_DESTROY:
        if (l) {
            for (int i = 0; i < l->n; i++)
                free(l->items[i]);
            free(l->items);
            free(l);
            w->ctl = 0;
        }
        return 0;
    case WM_SIZE:
        listShow(w, l);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        HDC dc = BeginPaint(h, 0);
        BYTE a = w->parent && cw_isdlg(w->parent) ? cw_sys[COLOR_EDIT] : cw_ctlattr(w);
        BYTE sa = focused(w) ? cw_sys[COLOR_HIGHLIGHT] : (BYTE)((a & 0xf0) | 0x0f);
        int vw = viewW(w), vh = viewH(w);
        RECT r;
        cw_rect(&r, 0, 0, vw, vh);
        CwFillRect(dc, &r, ' ', a);
        for (int row = 0; row < vh && l->top + row < l->n; row++) {
            int i = l->top + row;
            BYTE ia = i == l->sel ? sa : a;
            RECT ir;
            cw_rect(&ir, 0, row, vw, 1);
            CwFillRect(dc, &ir, ' ', ia);
            CwSetTextAttr(dc, ia);
            ir.left = 1;
            DrawText(dc, l->items[i], -1, &ir, DT_SINGLELINE | DT_NOPREFIX);
        }
        if (focused(w)) {
            CreateCaret(h, 0, 1, 1);
            SetCaretPos(1, l->sel >= 0 ? l->sel - l->top : 0);
            ShowCaret(h);
        }
        EndPaint(h, 0);
        return 0;
    }
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    case WM_SETFOCUS:
        if (l->sel < 0 && l->n)
            listSelect(w, l, 0, 1);
        cw_dirty = 1;
        return 0;
    case WM_KILLFOCUS:
        DestroyCaret();
        cw_dirty = 1;
        return 0;
    case WM_KEYDOWN: {
        int vh = viewH(w), s = l->sel;
        switch (wp) {
        case VK_UP: s--; break;
        case VK_DOWN: s++; break;
        case VK_PRIOR: s -= vh - 1; break;
        case VK_NEXT: s += vh - 1; break;
        case VK_HOME: s = 0; break;
        case VK_END: s = l->n - 1; break;
        default: return 0;
        }
        listSelect(w, l, s, 1);
        return 0;
    }
    case WM_CHAR:
        if (wp > ' ' && l->n) {
            char s[2] = { (char)wp, 0 };
            int i = listFind(l, l->sel, s, 0);
            if (i >= 0)
                listSelect(w, l, i, 1);
        } else if (wp == '\r' && (w->style & LBS_NOTIFY) && l->sel >= 0) {
            notify(w, LBN_DBLCLK);
        }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        SetFocus(h);
        int row = (short)HIWORD(lp);
        if (l->top + row < l->n)
            listSelect(w, l, l->top + row, 1);
        if (msg == WM_LBUTTONDBLCLK && (w->style & LBS_NOTIFY))
            notify(w, LBN_DBLCLK);
        return 0;
    }
    case WM_VSCROLL: {
        int vh = viewH(w), top = l->top;
        switch (wp) {
        case SB_LINEUP: top--; break;
        case SB_LINEDOWN: top++; break;
        case SB_PAGEUP: top -= vh; break;
        case SB_PAGEDOWN: top += vh; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: top = (short)LOWORD(lp); break;
        }
        l->top = top;
        int sel = l->sel;
        l->sel = -1;
        listShow(w, l);
        l->sel = sel;
        return 0;
    }
    case LB_ADDSTRING: {
        const char *s = (const char *)PTR(lp);
        int at = l->n;
        if (w->style & LBS_SORT)
            for (at = 0; at < l->n && lcmp(l->items[at], s) <= 0; at++)
                ;
        return listInsert(w, l, at, s);
    }
    case LB_INSERTSTRING:
        return listInsert(w, l, (short)wp, (const char *)PTR(lp));
    case LB_DELETESTRING: {
        int i = (int)wp;
        if (i < 0 || i >= l->n)
            return LB_ERR;
        free(l->items[i]);
        memmove(l->items + i, l->items + i + 1, (l->n - i - 1) * sizeof *l->items);
        l->n--;
        if (l->sel == i)
            l->sel = -1;
        else if (l->sel > i)
            l->sel--;
        listShow(w, l);
        return l->n;
    }
    case LB_RESETCONTENT:
        for (int i = 0; i < l->n; i++)
            free(l->items[i]);
        l->n = 0;
        l->sel = -1;
        l->top = 0;
        listShow(w, l);
        return 0;
    case LB_SETCURSEL:
        if ((short)wp < 0 || (int)wp >= l->n) {
            l->sel = -1;
            cw_dirty = 1;
            return LB_ERR;
        }
        listSelect(w, l, (int)wp, 0);
        return l->sel;
    case LB_GETCURSEL:
        return l->sel;
    case LB_GETCOUNT:
        return l->n;
    case LB_GETTEXT:
        if ((int)wp < 0 || (int)wp >= l->n || !lp)
            return LB_ERR;
        strcpy((char *)PTR(lp), l->items[wp]);
        return (LRESULT)strlen(l->items[wp]);
    case LB_GETTEXTLEN:
        if ((int)wp < 0 || (int)wp >= l->n)
            return LB_ERR;
        return (LRESULT)strlen(l->items[wp]);
    case LB_FINDSTRING:
    case LB_SELECTSTRING: {
        if (!l->n)
            return LB_ERR;
        int i = listFind(l, (short)wp < 0 ? l->n - 1 : (int)wp, (const char *)PTR(lp), 0);
        if (i >= 0 && msg == LB_SELECTSTRING)
            listSelect(w, l, i, 0);
        return i;
    }
    case LB_GETTOPINDEX:
        return l->top;
    case LB_SETTOPINDEX:
        l->top = (int)wp;
        listShow(w, l);
        return 0;
    case WM_ENABLE:
        cw_dirty = 1;
        return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

void
cw_register_controls(void)
{
    static const struct {
        const char *name;
        WNDPROC proc;
    } c[] = {
        { "BUTTON", buttonProc },
        { "STATIC", staticProc },
        { "EDIT", editProc },
        { "LISTBOX", listProc },
        { cw_dialogclass, cw_dialogproc },
    };
    for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
        WNDCLASS wc;
        memset(&wc, 0, sizeof wc);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = c[i].proc;
        wc.lpszClassName = c[i].name;
        if (c[i].proc == cw_dialogproc)
            wc.hbrBackground = (HBRUSH)(COLOR_DIALOG + 1);
        RegisterClass(&wc);
    }
}
