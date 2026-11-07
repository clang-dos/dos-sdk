/* Window classes, windows, painting, the message queue, input, timers,
 * carets, scroll bars and DefWindowProc.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "cwin-local.h"
#include <stdlib.h>
#include <string.h>

#define PTR(lp) ((void *)(uintptr_t)(lp))

struct wnd *cw_desktop, *cw_focus, *cw_active, *cw_capture;
char *cw_status;
BYTE cw_dirty = 1;

static struct cls *classes;
static struct wnd *painting;
static int cascade;
static WORD lastKey;
static DWORD msgTime;

/* ---- Classes. ---- */

static int
sameName(const char *a, const char *b)
{
    while (*a && cw_upper((unsigned char)*a) == cw_upper((unsigned char)*b))
        a++, b++;
    return !*a && !*b;
}

static struct cls *
findclass(LPCSTR name)
{
    for (struct cls *c = classes; c; c = c->next)
        if (sameName(c->name, name))
            return c;
    return 0;
}

BOOL
RegisterClass(const WNDCLASS *wc)
{
    cw_setup();
    if (!wc || !wc->lpszClassName || !wc->lpfnWndProc || findclass(wc->lpszClassName))
        return FALSE;
    struct cls *c = (struct cls *)calloc(1, sizeof *c);
    if (!c)
        return FALSE;
    c->wc = *wc;
    c->name = cw_strdup(wc->lpszClassName);
    c->wc.lpszClassName = c->name;
    c->next = classes;
    classes = c;
    return TRUE;
}

BOOL
UnregisterClass(LPCSTR name, HINSTANCE inst)
{
    (void)inst;
    struct cls *f = findclass(name);
    for (struct cls **p = &classes; *p; p = &(*p)->next)
        if (*p == f) {
            *p = f->next;
            free(f->name);
            free(f);
            return TRUE;
        }
    return FALSE;
}

BOOL
GetClassInfo(HINSTANCE inst, LPCSTR name, WNDCLASS *wc)
{
    (void)inst;
    cw_setup();
    struct cls *c = findclass(name);
    if (!c)
        return FALSE;
    *wc = c->wc;
    return TRUE;
}

int
GetClassName(HWND h, LPSTR buf, int max)
{
    struct wnd *w = W(h);
    if (!w || max <= 0)
        return 0;
    strncpy(buf, w->cls->name, max - 1);
    buf[max - 1] = 0;
    return (int)strlen(buf);
}

/* ---- Setup. ---- */

static LRESULT
desktopProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND) {
        struct wnd *w = W(h);
        cw_fill(&w->dcclip, 0, 0, cw_cols, cw_rows, 0xb0, cw_sys[COLOR_BACKGROUND]);
        return 1;
    }
    return DefWindowProc(h, msg, wp, lp);
}

static void
shutdownAtExit(void)
{
    CwShutdown();
}

void
cw_setup(void)
{
    if (cw_desktop)
        return;
    cw_init();
    static WNDCLASS dwc = { 0, desktopProc, 0, 0, 0, 0, 0, 0, 0, "#32769" };
    static struct cls dcls;
    dcls.wc = dwc;
    dcls.name = (char *)"#32769";
    struct wnd *d = (struct wnd *)calloc(1, sizeof *d);
    d->kind = OBJ_WND;
    d->cls = &dcls;
    d->proc = desktopProc;
    d->style = WS_VISIBLE;
    d->w = cw_cols;
    d->h = cw_rows;
    d->text = cw_strdup("");
    d->hwnd = cw_newhandle(d);
    cw_desktop = d;
    cw_register_controls();
    atexit(shutdownAtExit);
}

HWND GetDesktopWindow(void) { cw_setup(); return cw_desktop->hwnd; }

/* ---- Geometry. ---- */

struct wnd *
cw_top(struct wnd *w)
{
    while (w && w->parent && w->parent != cw_desktop)
        w = w->parent;
    return w;
}

static int
isTop(struct wnd *w)
{
    return w && w->parent == cw_desktop;
}

int
cw_hasframe(struct wnd *w)
{
    return (w->style & (WS_BORDER | WS_DLGFRAME | WS_THICKFRAME)) != 0;
}

void
cw_clientinset(struct wnd *w, RECT *r)
{
    cw_rect(r, 0, 0, w->w, w->h);
    if (w == cw_desktop)
        return;
    if (cw_hasframe(w)) {
        r->left++;
        r->top++;
        r->right--;
        r->bottom--;
    }
    if (isTop(w) && w->menu)
        r->top++;
    if (w->style & WS_VSCROLL)
        r->right--;
    if (r->right < r->left)
        r->right = r->left;
    if (r->bottom < r->top)
        r->bottom = r->top;
}

void
cw_winrect(struct wnd *w, RECT *r)
{
    if (w == cw_desktop) {
        cw_rect(r, 0, 0, cw_cols, cw_rows);
        return;
    }
    RECT pc;
    cw_clientrect(w->parent, &pc);
    cw_rect(r, pc.left + w->x, pc.top + w->y, w->w, w->h);
}

void
cw_clientrect(struct wnd *w, RECT *r)
{
    RECT wr, in;
    cw_winrect(w, &wr);
    cw_clientinset(w, &in);
    cw_rect(r, wr.left + in.left, wr.top + in.top, in.right - in.left, in.bottom - in.top);
}

void
GetWindowRect(HWND h, LPRECT rc)
{
    struct wnd *w = W(h);
    if (w)
        cw_winrect(w, rc);
}

void
GetClientRect(HWND h, LPRECT rc)
{
    struct wnd *w = W(h);
    if (!w)
        return;
    RECT in;
    cw_clientinset(w, &in);
    cw_rect(rc, 0, 0, in.right - in.left, in.bottom - in.top);
}

void
ClientToScreen(HWND h, LPPOINT pt)
{
    struct wnd *w = W(h);
    if (!w)
        return;
    RECT c;
    cw_clientrect(w, &c);
    pt->x += c.left;
    pt->y += c.top;
}

void
ScreenToClient(HWND h, LPPOINT pt)
{
    struct wnd *w = W(h);
    if (!w)
        return;
    RECT c;
    cw_clientrect(w, &c);
    pt->x -= c.left;
    pt->y -= c.top;
}

static int
inRect(const RECT *r, int x, int y)
{
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

static int
visible(struct wnd *w)
{
    for (; w; w = w->parent)
        if (!(w->style & WS_VISIBLE))
            return 0;
    return 1;
}

/* The innermost visible window at a screen cell. */
static struct wnd *
hit(struct wnd *p, int x, int y)
{
    for (struct wnd *c = p->last; c; c = c->prev) {
        if (!(c->style & WS_VISIBLE))
            continue;
        RECT r;
        cw_winrect(c, &r);
        if (!inRect(&r, x, y))
            continue;
        cw_clientrect(c, &r);
        return inRect(&r, x, y) ? hit(c, x, y) : c;
    }
    return p;
}

HWND
WindowFromPoint(POINT pt)
{
    cw_setup();
    return hit(cw_desktop, pt.x, pt.y)->hwnd;
}

/* ---- Brushes and colors. ---- */

static int
brushAttr(HBRUSH br)
{
    if (br & 0x100)
        return br & 0xff;
    if (br >= 1 && br <= CW_NCOLORS)
        return cw_sys[br - 1];
    return -1;
}

static BYTE
bgattr(struct wnd *w)
{
    if (w->isDialog)
        return cw_sys[COLOR_DIALOG];
    int a = brushAttr(w->cls->wc.hbrBackground);
    return a >= 0 ? (BYTE)a : cw_sys[COLOR_WINDOW];
}

BYTE
cw_ctlattr(struct wnd *w)
{
    return w->parent ? bgattr(w->parent) : cw_sys[COLOR_WINDOW];
}

int
cw_isdlg(struct wnd *w)
{
    return w && w->isDialog;
}

/* ---- Creating and destroying. ---- */

static void
unlink(struct wnd *w)
{
    struct wnd *p = w->parent;
    if (!p)
        return;
    if (w->prev)
        w->prev->next = w->next;
    else
        p->first = w->next;
    if (w->next)
        w->next->prev = w->prev;
    else
        p->last = w->prev;
    w->next = w->prev = 0;
}

static void
linkTop(struct wnd *w)
{
    struct wnd *p = w->parent;
    w->prev = p->last;
    w->next = 0;
    if (p->last)
        p->last->next = w;
    else
        p->first = w;
    p->last = w;
}

static void
sendSize(struct wnd *w)
{
    RECT in;
    cw_clientinset(w, &in);
    cw_send(w, WM_SIZE, w->zoomed ? SIZE_MAXIMIZED : SIZE_RESTORED,
            MAKELONG(in.right - in.left, in.bottom - in.top));
}

HWND
CreateWindowEx(DWORD exStyle, LPCSTR clsName, LPCSTR title, DWORD style, int x, int y, int cx,
               int cy, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    cw_setup();
    struct cls *c = findclass(clsName);
    if (!c)
        return 0;
    struct wnd *pw = parent ? W(parent) : 0;
    if ((style & WS_CHILD) && !pw)
        return 0;
    int extra = c->wc.cbWndExtra > 0 ? c->wc.cbWndExtra : 0;
    struct wnd *w = (struct wnd *)calloc(1, sizeof *w + extra);
    if (!w)
        return 0;
    w->kind = OBJ_WND;
    w->hwnd = cw_newhandle(w);
    if (!w->hwnd) {
        free(w);
        return 0;
    }
    w->cls = c;
    w->proc = c->wc.lpfnWndProc;
    if (!(style & (WS_CHILD | WS_POPUP)))
        style |= WS_CAPTION;
    w->style = style & ~(WS_VISIBLE | WS_MAXIMIZE);
    w->exstyle = exStyle;
    w->text = cw_strdup(title);
    w->sbmax = 100;
    if (style & WS_CHILD) {
        w->parent = pw;
        w->id = (UINT)menu;
    } else {
        w->parent = cw_desktop;
        w->owner = cw_top(pw);
        if (M(menu))
            w->menu = menu;
    }
    RECT pc;
    cw_clientinset(w->parent, &pc);
    int pwid = pc.right - pc.left, phei = pc.bottom - pc.top;
    if (x == CW_USEDEFAULT) {
        x = 2 + 2 * cascade;
        y = 1 + cascade;
        cascade = (cascade + 1) % 6;
    }
    if (cx == CW_USEDEFAULT) {
        cx = pwid * 3 / 4;
        cy = phei * 2 / 3;
    }
    w->x = x;
    w->y = y;
    w->w = cx;
    w->h = cy;
    linkTop(w);
    CREATESTRUCT cs = { param, inst, menu, parent, cy, cx, y, x, (LONG)style, title, clsName, exStyle };
    HWND h = w->hwnd;
    if (cw_send(w, WM_CREATE, 0, (LPARAM)(uintptr_t)&cs) == -1) {
        DestroyWindow(h);
        return 0;
    }
    if (!W(h))
        return 0;
    sendSize(w);
    if (style & WS_MAXIMIZE)
        ShowWindow(h, SW_MAXIMIZE);
    else if (style & WS_VISIBLE)
        ShowWindow(h, SW_SHOW);
    cw_dirty = 1;
    return h;
}

HWND
CreateWindow(LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int cx, int cy, HWND parent,
             HMENU menu, HINSTANCE inst, LPVOID param)
{
    return CreateWindowEx(0, cls, title, style, x, y, cx, cy, parent, menu, inst, param);
}

static int
within(struct wnd *w, struct wnd *anc)
{
    for (; w; w = w->parent)
        if (w == anc)
            return 1;
    return 0;
}

static void killTimers(struct wnd *w);
static struct wnd *caretOwner;

/* The topmost visible, enabled top-level window, preferring w. */
static struct wnd *
nextActive(struct wnd *w)
{
    if (w && isTop(w) && (w->style & WS_VISIBLE) && !(w->style & WS_DISABLED) && !w->destroyed)
        return w;
    for (struct wnd *c = cw_desktop->last; c; c = c->prev)
        if ((c->style & WS_VISIBLE) && !(c->style & WS_DISABLED) && !c->destroyed)
            return c;
    return 0;
}

static void
destroyTree(struct wnd *w)
{
    w->destroyed = 1;
    cw_send(w, WM_DESTROY, 0, 0);
    while (w->first) {
        struct wnd *c = w->first;
        unlink(c);
        c->parent = 0;
        destroyTree(c);
    }
    killTimers(w);
    if (caretOwner == w)
        caretOwner = 0;
    if (cw_capture == w)
        cw_capture = 0;
    if (w->menu && !(w->style & WS_CHILD))
        DestroyMenu(w->menu);
    free(w->text);
    cw_freehandle(w->hwnd);
    free(w);
}

BOOL
DestroyWindow(HWND h)
{
    struct wnd *w = W(h);
    if (!w || w == cw_desktop || w->destroyed)
        return FALSE;
    /* Owned windows go first. */
    if (isTop(w)) {
        struct wnd *c = cw_desktop->first;
        while (c) {
            struct wnd *n = c->next;
            if (c->owner == w && !c->destroyed) {
                DestroyWindow(c->hwnd);
                n = cw_desktop->first;
            }
            c = n;
        }
    }
    int wasActive = cw_active == w;
    struct wnd *owner = w->owner, *parent = w->parent;
    if (cw_focus && within(cw_focus, w)) {
        struct wnd *f = cw_focus;
        cw_focus = 0;
        cw_send(f, WM_KILLFOCUS, 0, 0);
    }
    if (isTop(w))
        w->style &= ~WS_VISIBLE;
    for (struct wnd *c = cw_desktop->first; c; c = c->next)
        if (c->lastFocus && within(c->lastFocus, w))
            c->lastFocus = 0;
    unlink(w);
    w->parent = parent; /* for WM_DESTROY handlers that look at it */
    destroyTree(w);
    if (wasActive) {
        cw_active = 0;
        struct wnd *n = nextActive(owner);
        if (n)
            SetActiveWindow(n->hwnd);
    }
    cw_dirty = 1;
    return TRUE;
}

BOOL IsWindow(HWND h) { struct wnd *w = W(h); return w && !w->destroyed; }

/* ---- Showing, moving, z-order, activation and focus. ---- */

static void
zoom(struct wnd *w, int on)
{
    if (on && !w->zoomed) {
        cw_rect(&w->normal, w->x, w->y, w->w, w->h);
        RECT pc;
        cw_clientinset(w->parent, &pc);
        w->zoomed = 1;
        MoveWindow(w->hwnd, 0, 0, pc.right - pc.left, pc.bottom - pc.top, TRUE);
    } else if (!on && w->zoomed) {
        w->zoomed = 0;
        MoveWindow(w->hwnd, w->normal.left, w->normal.top, w->normal.right - w->normal.left,
                   w->normal.bottom - w->normal.top, TRUE);
    }
}

BOOL
ShowWindow(HWND h, int cmd)
{
    struct wnd *w = W(h);
    if (!w)
        return FALSE;
    BOOL was = (w->style & WS_VISIBLE) != 0;
    if (cmd == SW_HIDE) {
        if (!was)
            return FALSE;
        w->style &= ~WS_VISIBLE;
        cw_send(w, WM_SHOWWINDOW, FALSE, 0);
        if (cw_focus && within(cw_focus, w))
            SetFocus(isTop(w) ? 0 : w->parent->hwnd);
        if (cw_active == w) {
            cw_active = 0;
            struct wnd *n = nextActive(w->owner);
            if (n)
                SetActiveWindow(n->hwnd);
        }
        cw_dirty = 1;
        return was;
    }
    if (!was) {
        w->style |= WS_VISIBLE;
        cw_send(w, WM_SHOWWINDOW, TRUE, 0);
    }
    if (cmd == SW_MAXIMIZE)
        zoom(w, 1);
    else if (cmd == SW_RESTORE || cmd == SW_SHOWNORMAL)
        zoom(w, 0);
    if (isTop(w) && cmd != SW_SHOWNOACTIVATE && !(w->style & WS_DISABLED))
        SetActiveWindow(h);
    cw_dirty = 1;
    return was;
}

BOOL IsWindowVisible(HWND h) { struct wnd *w = W(h); return w && visible(w); }
BOOL IsZoomed(HWND h) { struct wnd *w = W(h); return w && w->zoomed; }

BOOL
EnableWindow(HWND h, BOOL enable)
{
    struct wnd *w = W(h);
    if (!w)
        return FALSE;
    BOOL wasDisabled = (w->style & WS_DISABLED) != 0;
    if (enable)
        w->style &= ~WS_DISABLED;
    else {
        w->style |= WS_DISABLED;
        if (cw_focus && within(cw_focus, w))
            SetFocus(0);
    }
    if (wasDisabled == !enable)
        return wasDisabled;
    cw_send(w, WM_ENABLE, enable, 0);
    cw_dirty = 1;
    return wasDisabled;
}

BOOL IsWindowEnabled(HWND h) { struct wnd *w = W(h); return w && !(w->style & WS_DISABLED); }

BOOL
MoveWindow(HWND h, int x, int y, int cx, int cy, BOOL repaint)
{
    struct wnd *w = W(h);
    if (!w || w == cw_desktop)
        return FALSE;
    (void)repaint;
    int sized = cx != w->w || cy != w->h, moved = x != w->x || y != w->y;
    w->x = x;
    w->y = y;
    w->w = cx < 0 ? 0 : cx;
    w->h = cy < 0 ? 0 : cy;
    if (moved) {
        RECT in;
        cw_clientinset(w, &in);
        cw_send(w, WM_MOVE, 0, MAKELONG(x + in.left, y + in.top));
    }
    if (sized)
        sendSize(w);
    cw_dirty = 1;
    return TRUE;
}

static void
raise(struct wnd *w)
{
    unlink(w);
    linkTop(w);
}

BOOL
BringWindowToTop(HWND h)
{
    struct wnd *w = W(h);
    if (!w || w == cw_desktop)
        return FALSE;
    raise(w);
    if (isTop(w)) {
        /* Its owned windows stay above it. */
        struct wnd *c = cw_desktop->first, *stop = w;
        while (c && c != stop) {
            struct wnd *n = c->next;
            if (c->owner == w)
                raise(c);
            c = n;
        }
    }
    cw_dirty = 1;
    return TRUE;
}

HWND
SetActiveWindow(HWND h)
{
    struct wnd *w = cw_top(W(h)), *old = cw_active;
    if (!w || w == cw_desktop)
        return old ? old->hwnd : 0;
    if (w == old)
        return old->hwnd;
    cw_active = w;
    BringWindowToTop(w->hwnd);
    if (old && W(old->hwnd))
        cw_send(old, WM_ACTIVATE, WA_INACTIVE, MAKELONG(w->hwnd, 0));
    if (cw_active == w)
        cw_send(w, WM_ACTIVATE, WA_ACTIVE, MAKELONG(old ? old->hwnd : 0, 0));
    cw_dirty = 1;
    return old ? old->hwnd : 0;
}

HWND GetActiveWindow(void) { return cw_active ? cw_active->hwnd : 0; }

HWND
SetFocus(HWND h)
{
    struct wnd *w = W(h), *old = cw_focus;
    HWND oldh = old ? old->hwnd : 0;
    if (w == old || w == cw_desktop)
        return oldh;
    if (w && (w->style & WS_DISABLED))
        return oldh;
    if (old) {
        cw_focus = 0;
        cw_send(old, WM_KILLFOCUS, h, 0);
    }
    cw_focus = w;
    if (w) {
        struct wnd *t = cw_top(w);
        t->lastFocus = w;
        if (cw_active != t)
            SetActiveWindow(t->hwnd);
        if (cw_focus == w)
            cw_send(w, WM_SETFOCUS, W(oldh) ? oldh : 0, 0);
    }
    cw_dirty = 1;
    return W(oldh) ? oldh : 0;
}

HWND GetFocus(void) { return cw_focus ? cw_focus->hwnd : 0; }

HWND
SetCapture(HWND h)
{
    struct wnd *old = cw_capture;
    cw_capture = W(h);
    return old ? old->hwnd : 0;
}

void ReleaseCapture(void) { cw_capture = 0; }
HWND GetCapture(void) { return cw_capture ? cw_capture->hwnd : 0; }

HWND
GetParent(HWND h)
{
    struct wnd *w = W(h);
    if (!w)
        return 0;
    if (w->style & WS_CHILD)
        return w->parent ? w->parent->hwnd : 0;
    return w->owner ? w->owner->hwnd : 0;
}

HWND
GetWindow(HWND h, UINT cmd)
{
    struct wnd *w = W(h), *r = 0;
    if (!w)
        return 0;
    switch (cmd) {
    case GW_HWNDFIRST:
        r = w->parent ? w->parent->last : 0;
        break;
    case GW_HWNDLAST:
        r = w->parent ? w->parent->first : 0;
        break;
    case GW_HWNDNEXT: /* below, as in Windows: the z-order goes down */
        r = w->prev;
        break;
    case GW_HWNDPREV:
        r = w->next;
        break;
    case GW_OWNER:
        r = w->owner;
        break;
    case GW_CHILD:
        r = w->last;
        break;
    }
    return r ? r->hwnd : 0;
}

HWND
GetTopWindow(HWND h)
{
    struct wnd *w = h ? W(h) : (cw_setup(), cw_desktop);
    return w && w->last ? w->last->hwnd : 0;
}

HWND
FindWindow(LPCSTR cls, LPCSTR title)
{
    cw_setup();
    for (struct wnd *c = cw_desktop->last; c; c = c->prev)
        if ((!cls || sameName(c->cls->name, cls)) && (!title || !strcmp(c->text, title)))
            return c->hwnd;
    return 0;
}

/* ---- Text and window data. ---- */

void SetWindowText(HWND h, LPCSTR text) { SendMessage(h, WM_SETTEXT, 0, (LPARAM)(uintptr_t)text); }

int
GetWindowText(HWND h, LPSTR buf, int max)
{
    return (int)SendMessage(h, WM_GETTEXT, max, (LPARAM)(uintptr_t)buf);
}

int GetWindowTextLength(HWND h) { return (int)SendMessage(h, WM_GETTEXTLENGTH, 0, 0); }

static LONG *
dlgLong(struct wnd *w, int i)
{
    if (!w->isDialog)
        return 0;
    switch (i) {
    case DWL_MSGRESULT:
        return &w->dlgresult;
    case DWL_USER:
        return &w->dlguser;
    }
    return 0;
}

LONG
GetWindowLong(HWND h, int i)
{
    struct wnd *w = W(h);
    if (!w)
        return 0;
    LONG *d = dlgLong(w, i);
    if (d)
        return *d;
    if (w->isDialog && i == DWL_DLGPROC)
        return (LONG)w->dlgproc;
    switch (i) {
    case GWL_WNDPROC:
        return (LONG)w->proc;
    case GWL_STYLE:
        return (LONG)w->style;
    case GWL_EXSTYLE:
        return (LONG)w->exstyle;
    case GWL_ID:
        return w->id;
    case GWL_USERDATA:
        return w->userdata;
    case GWW_HWNDPARENT:
        return GetParent(h);
    case GWW_HINSTANCE:
        return 0;
    }
    if (i >= 0 && i + 4 <= w->cls->wc.cbWndExtra) {
        LONG v;
        memcpy(&v, w->extra + i, 4);
        return v;
    }
    return 0;
}

LONG
SetWindowLong(HWND h, int i, LONG v)
{
    struct wnd *w = W(h);
    if (!w)
        return 0;
    LONG old = GetWindowLong(h, i);
    LONG *d = dlgLong(w, i);
    if (d) {
        *d = v;
        return old;
    }
    if (w->isDialog && i == DWL_DLGPROC) {
        w->dlgproc = (DLGPROC)v;
        return old;
    }
    switch (i) {
    case GWL_WNDPROC:
        w->proc = (WNDPROC)v;
        break;
    case GWL_STYLE:
        w->style = (DWORD)v;
        cw_dirty = 1;
        break;
    case GWL_EXSTYLE:
        w->exstyle = (DWORD)v;
        break;
    case GWL_ID:
        w->id = (UINT)v;
        break;
    case GWL_USERDATA:
        w->userdata = v;
        break;
    default:
        if (i >= 0 && i + 4 <= w->cls->wc.cbWndExtra)
            memcpy(w->extra + i, &v, 4);
    }
    return old;
}

WORD
GetWindowWord(HWND h, int i)
{
    struct wnd *w = W(h);
    if (!w)
        return 0;
    if (i < 0)
        return (WORD)GetWindowLong(h, i);
    WORD v = 0;
    if (i + 2 <= w->cls->wc.cbWndExtra)
        memcpy(&v, w->extra + i, 2);
    return v;
}

WORD
SetWindowWord(HWND h, int i, WORD v)
{
    struct wnd *w = W(h);
    if (!w)
        return 0;
    WORD old = GetWindowWord(h, i);
    if (i < 0)
        SetWindowLong(h, i, v);
    else if (i + 2 <= w->cls->wc.cbWndExtra)
        memcpy(w->extra + i, &v, 2);
    return old;
}

/* ---- Scroll bars. ---- */

void
cw_scrollbar_rect(struct wnd *w, RECT *r)
{
    RECT wr, in;
    cw_winrect(w, &wr);
    cw_clientinset(w, &in);
    cw_rect(r, wr.left + in.right, wr.top + in.top, 1, in.bottom - in.top);
}

static int
thumbRow(struct wnd *w, int len)
{
    if (w->sbmax <= w->sbmin || len <= 0)
        return 0;
    long p = (long)(w->sbpos - w->sbmin) * (len - 1) / (w->sbmax - w->sbmin);
    return (int)p;
}

int
SetScrollPos(HWND h, int bar, int pos, BOOL redraw)
{
    struct wnd *w = W(h);
    (void)bar;
    (void)redraw;
    if (!w)
        return 0;
    int old = w->sbpos;
    if (pos < w->sbmin)
        pos = w->sbmin;
    if (pos > w->sbmax)
        pos = w->sbmax;
    w->sbpos = pos;
    cw_dirty = 1;
    return old;
}

int GetScrollPos(HWND h, int bar) { struct wnd *w = W(h); (void)bar; return w ? w->sbpos : 0; }

void
SetScrollRange(HWND h, int bar, int mn, int mx, BOOL redraw)
{
    struct wnd *w = W(h);
    if (!w)
        return;
    w->sbmin = mn;
    w->sbmax = mx;
    SetScrollPos(h, bar, w->sbpos, redraw);
}

void
GetScrollRange(HWND h, int bar, int *mn, int *mx)
{
    struct wnd *w = W(h);
    (void)bar;
    if (!w)
        return;
    *mn = w->sbmin;
    *mx = w->sbmax;
}

/* ---- Painting. ---- */

static void
setdc(struct wnd *w)
{
    RECT c;
    cw_clientrect(w, &c);
    w->dcx = c.left;
    w->dcy = c.top;
    for (struct wnd *p = w->parent; p; p = p->parent) {
        RECT pc;
        cw_clientrect(p, &pc);
        cw_intersect(&c, &pc);
    }
    w->dcclip = c;
    w->attr = bgattr(w);
}

static BYTE
frameAttr(struct wnd *w)
{
    if (!isTop(w))
        return cw_ctlattr(w);
    if (w->isDialog || (w->style & WS_POPUP))
        return cw_sys[COLOR_DIALOG];
    return w == cw_active ? cw_sys[COLOR_ACTIVEBORDER] : cw_sys[COLOR_INACTIVEBORDER];
}

static void
ncpaint(struct wnd *w, const RECT *clip)
{
    RECT wr;
    cw_winrect(w, &wr);
    BYTE fa = frameAttr(w);
    if (cw_hasframe(w)) {
        cw_box(clip, wr.left, wr.top, w->w, w->h, fa, FALSE);
        if ((w->style & WS_CAPTION) == WS_CAPTION && w->text[0]) {
            int n = cw_imin((int)strlen(w->text), w->w - 4);
            int x = wr.left + (w->w - n - 2) / 2;
            BYTE ca = fa;
            if (isTop(w) && !w->isDialog && !(w->style & WS_POPUP))
                ca = w == cw_active ? cw_sys[COLOR_ACTIVECAPTION] : cw_sys[COLOR_INACTIVECAPTION];
            if (n > 0) {
                cw_put(clip, x, wr.top, ' ', ca);
                cw_text(clip, x + 1, wr.top, w->text, n, ca, 0);
                cw_put(clip, x + n + 1, wr.top, ' ', ca);
            }
        }
        if (isTop(w) && !(w->style & WS_POPUP) && (w->style & WS_CAPTION) == WS_CAPTION && w->w >= 10) {
            if (w->style & WS_SYSMENU)
                cw_text(clip, wr.left + 1, wr.top, "[\xfe]", 3, fa, 0);
            if (w->style & WS_MAXIMIZEBOX)
                cw_text(clip, wr.right - 4, wr.top, w->zoomed ? "[\x12]" : "[\x18]", 3, fa, 0);
        }
    }
    if (isTop(w) && w->menu)
        cw_menubar_paint(w, clip);
    if (w->style & WS_VSCROLL) {
        RECT s;
        cw_scrollbar_rect(w, &s);
        int len = s.bottom - s.top;
        BYTE a = cw_sys[COLOR_SCROLLBAR];
        if (len >= 2) {
            cw_put(clip, s.left, s.top, 0x18, a);
            cw_fill(clip, s.left, s.top + 1, 1, len - 2, 0xb0, a);
            cw_put(clip, s.left, s.bottom - 1, 0x19, a);
            if (len >= 3)
                cw_put(clip, s.left, s.top + 1 + thumbRow(w, len - 2), 0x20,
                       (BYTE)((a >> 4) | (a << 4)));
        }
    }
}

static void
paintWindow(struct wnd *w, const RECT *clip)
{
    if (!(w->style & WS_VISIBLE) || w->destroyed)
        return;
    RECT wr, c;
    cw_winrect(w, &wr);
    c = wr;
    cw_intersect(&c, clip);
    if (isTop(w) && (w->style & WS_POPUP) && cw_hasframe(w)) {
        cw_shade(clip, wr.right, wr.top + 1, 2, w->h);
        cw_shade(clip, wr.left + 2, wr.bottom, w->w, 1);
    }
    if (c.right <= c.left || c.bottom <= c.top)
        return;
    ncpaint(w, &c);
    RECT cr;
    cw_clientrect(w, &cr);
    cw_intersect(&cr, &c);
    RECT cc = cr;
    RECT full;
    cw_clientrect(w, &full);
    w->dcx = full.left;
    w->dcy = full.top;
    w->dcclip = cc;
    w->attr = bgattr(w);
    HWND h = w->hwnd;
    painting = w;
    cw_send(w, WM_ERASEBKGND, h, 0);
    if (!W(h))
        return;
    w->dcx = full.left;
    w->dcy = full.top;
    w->dcclip = cc;
    w->attr = bgattr(w);
    painting = w;
    cw_send(w, WM_PAINT, 0, 0);
    painting = 0;
    if (!W(h))
        return;
    for (struct wnd *k = w->first; k; k = k->next)
        paintWindow(k, &cc);
}

static void
statusPaint(void)
{
    if (!cw_status)
        return;
    RECT all;
    cw_rect(&all, 0, 0, cw_cols, cw_rows);
    int y = cw_rows - 1;
    BYTE a = cw_sys[COLOR_STATUS];
    cw_fill(&all, 0, y, cw_cols, 1, ' ', a);
    cw_text(&all, 1, y, cw_status, -1, a, 0);
    const char *t = strchr(cw_status, '\t');
    if (t) {
        int n = (int)strlen(t + 1);
        cw_text(&all, cw_cols - 1 - n, y, t + 1, -1, a, 0);
    }
}

/* The caret, if its window shows at its place. */
static int caretX, caretY, caretShown;

static void
caretCell(int *x, int *y)
{
    *x = *y = -1;
    struct wnd *w = caretOwner;
    if (!w || !caretShown || !visible(w) || w->destroyed)
        return;
    RECT c;
    cw_clientrect(w, &c);
    int cx = c.left + caretX, cy = c.top + caretY;
    if (!inRect(&c, cx, cy) || hit(cw_desktop, cx, cy) != w)
        return;
    *x = cx;
    *y = cy;
}

void
cw_paintall(void)
{
    RECT all;
    cw_rect(&all, 0, 0, cw_cols, cw_rows);
    cw_dirty = 0;
    cw_desktop->h = cw_status ? cw_rows - 1 : cw_rows;
    paintWindow(cw_desktop, &all);
    statusPaint();
    cw_menu_overlay();
}

static void
show(int withCaret)
{
    if (cw_dirty)
        cw_paintall();
    int x = -1, y = -1;
    if (withCaret)
        caretCell(&x, &y);
    cw_flush(x, y);
}

HDC
BeginPaint(HWND h, LPPAINTSTRUCT ps)
{
    struct wnd *w = W(h);
    if (!w)
        return 0;
    if (painting != w)
        setdc(w);
    if (ps) {
        ps->hdc = h;
        ps->fErase = FALSE;
        RECT in;
        cw_clientinset(w, &in);
        cw_rect(&ps->rcPaint, 0, 0, in.right - in.left, in.bottom - in.top);
    }
    return h;
}

void EndPaint(HWND h, const PAINTSTRUCT *ps) { (void)h; (void)ps; }

HDC
GetDC(HWND h)
{
    cw_setup();
    struct wnd *w = h ? W(h) : cw_desktop;
    if (!w)
        return 0;
    if (painting != w)
        setdc(w);
    return w->hwnd;
}

int ReleaseDC(HWND h, HDC hdc) { (void)h; (void)hdc; return 1; }

void InvalidateRect(HWND h, const RECT *rc, BOOL erase) { (void)h; (void)rc; (void)erase; cw_dirty = 1; }
void ValidateRect(HWND h, const RECT *rc) { (void)h; (void)rc; }

void
UpdateWindow(HWND h)
{
    (void)h;
    if (cw_dirty)
        show(1);
}

COLORREF
SetTextColor(HDC hdc, COLORREF c)
{
    struct wnd *w = W(hdc);
    if (!w)
        return 0;
    COLORREF old = w->attr & 15;
    w->attr = (BYTE)((w->attr & 0xf0) | (c & 15));
    return old;
}

COLORREF
SetBkColor(HDC hdc, COLORREF c)
{
    struct wnd *w = W(hdc);
    if (!w)
        return 0;
    COLORREF old = w->attr >> 4;
    w->attr = (BYTE)((w->attr & 15) | (c & 15) << 4);
    return old;
}

BYTE
CwSetTextAttr(HDC hdc, BYTE a)
{
    struct wnd *w = W(hdc);
    if (!w)
        return 0;
    BYTE old = w->attr;
    w->attr = a;
    return old;
}

BOOL
TextOut(HDC hdc, int x, int y, LPCSTR s, int n)
{
    struct wnd *w = W(hdc);
    if (!w)
        return FALSE;
    for (int i = 0; i < n && s[i]; i++)
        cw_put(&w->dcclip, w->dcx + x + i, w->dcy + y, (BYTE)s[i], w->attr);
    return TRUE;
}

void
CwPutChar(HDC hdc, int x, int y, BYTE ch, BYTE a)
{
    struct wnd *w = W(hdc);
    if (w)
        cw_put(&w->dcclip, w->dcx + x, w->dcy + y, ch, a);
}

void
CwFillRect(HDC hdc, const RECT *rc, BYTE ch, BYTE a)
{
    struct wnd *w = W(hdc);
    if (w)
        cw_fill(&w->dcclip, w->dcx + rc->left, w->dcy + rc->top, rc->right - rc->left,
                rc->bottom - rc->top, ch, a);
}

int
FillRect(HDC hdc, const RECT *rc, HBRUSH br)
{
    int a = brushAttr(br);
    if (a < 0)
        return 0;
    CwFillRect(hdc, rc, ' ', (BYTE)a);
    return 1;
}

void
CwDrawBox(HDC hdc, const RECT *rc, BYTE a, BOOL dbl)
{
    struct wnd *w = W(hdc);
    if (w)
        cw_box(&w->dcclip, w->dcx + rc->left, w->dcy + rc->top, rc->right - rc->left,
               rc->bottom - rc->top, a, dbl);
}

/* The length of the line starting at s that fits in width (breaking at a
 * space with DT_WORDBREAK), and where the next one starts. */
static int
lineSpan(const char *s, int n, int width, UINT fmt, int prefix, int *next)
{
    int i = 0, cols = 0, lastSpace = -1;
    while (i < n && s[i] != '\n' && s[i] != '\r') {
        if (prefix && s[i] == '&' && i + 1 < n && s[i + 1] != '&') {
            i++;
            continue;
        }
        if (s[i] == ' ')
            lastSpace = i;
        if ((fmt & DT_WORDBREAK) && cols >= width && width > 0) {
            if (lastSpace > 0) {
                *next = lastSpace + 1;
                return lastSpace;
            }
            *next = i;
            return i;
        }
        if (prefix && s[i] == '&')
            i++;
        cols++;
        i++;
    }
    *next = i;
    if (i < n && s[i] == '\r')
        (*next)++;
    if (*next < n && s[*next] == '\n')
        (*next)++;
    return i;
}

int
DrawText(HDC hdc, LPCSTR s, int n, LPRECT rc, UINT fmt)
{
    struct wnd *w = W(hdc);
    if (!w || !s)
        return 0;
    if (n < 0)
        n = (int)strlen(s);
    int prefix = !(fmt & DT_NOPREFIX);
    int width = rc->right - rc->left;
    BYTE a = w->attr, hot = (BYTE)((a & 0xf0) | 0x0f);
    if (hot == a)
        hot = (BYTE)((a & 0xf0) | 0x0e);
    /* Count the lines first, for DT_VCENTER and DT_BOTTOM. */
    int lines = 0, maxw = 0;
    for (int p = 0; p < n;) {
        int next, len = lineSpan(s + p, n - p, width, fmt, prefix, &next);
        int cw = 0;
        for (int i = 0; i < len; i++) {
            if (prefix && s[p + i] == '&' && i + 1 < len && s[p + i + 1] != '&')
                continue;
            if (prefix && s[p + i] == '&')
                i++;
            cw++;
        }
        maxw = cw_imax(maxw, cw);
        lines++;
        if (fmt & DT_SINGLELINE)
            break;
        p += next ? next : 1;
    }
    if (fmt & DT_CALCRECT) {
        rc->right = rc->left + maxw;
        rc->bottom = rc->top + lines;
        return lines;
    }
    int y = rc->top;
    if (fmt & DT_SINGLELINE) {
        if (fmt & DT_VCENTER)
            y = rc->top + (rc->bottom - rc->top - 1) / 2;
        else if (fmt & DT_BOTTOM)
            y = rc->bottom - 1;
    }
    RECT clip = w->dcclip, r;
    cw_rect(&r, w->dcx + rc->left, w->dcy + rc->top, width, rc->bottom - rc->top);
    cw_intersect(&clip, &r);
    for (int p = 0, l = 0; p < n && l < lines; l++, y++) {
        int next, len = lineSpan(s + p, n - p, width, fmt, prefix, &next);
        int cw = 0;
        for (int i = 0; i < len; i++) {
            if (prefix && s[p + i] == '&' && i + 1 < len && s[p + i + 1] != '&')
                continue;
            if (prefix && s[p + i] == '&')
                i++;
            cw++;
        }
        int x = rc->left;
        if (fmt & DT_CENTER)
            x = rc->left + (width - cw) / 2;
        else if (fmt & DT_RIGHT)
            x = rc->right - cw;
        int cx = w->dcx + x;
        for (int i = 0; i < len; i++) {
            BYTE at = a;
            if (prefix && s[p + i] == '&') {
                if (i + 1 < len && s[p + i + 1] != '&')
                    at = hot;
                i++;
                if (i >= len)
                    break;
            }
            cw_put(&clip, cx++, w->dcy + y, (BYTE)s[p + i], at);
        }
        p += next ? next : 1;
    }
    return lines;
}

/* ---- Carets. ---- */

BOOL
CreateCaret(HWND h, HBITMAP bm, int w, int ht)
{
    (void)bm;
    (void)w;
    (void)ht;
    caretOwner = W(h);
    caretShown = 0;
    caretX = caretY = 0;
    return caretOwner != 0;
}

BOOL DestroyCaret(void) { caretOwner = 0; caretShown = 0; return TRUE; }

BOOL
ShowCaret(HWND h)
{
    if (!caretOwner || (h && W(h) != caretOwner))
        return FALSE;
    caretShown = 1;
    return TRUE;
}

BOOL
HideCaret(HWND h)
{
    if (!caretOwner || (h && W(h) != caretOwner))
        return FALSE;
    caretShown = 0;
    return TRUE;
}

BOOL SetCaretPos(int x, int y) { caretX = x; caretY = y; return TRUE; }

BOOL
GetCaretPos(LPPOINT pt)
{
    pt->x = caretX;
    pt->y = caretY;
    return TRUE;
}

/* ---- Timers. ---- */

#define NTIMERS 16
static struct timer {
    struct wnd *w;
    UINT id;
    uint16_t every, next;
    TIMERPROC proc;
    BYTE used;
} timers[NTIMERS];

UINT
SetTimer(HWND h, UINT id, UINT ms, TIMERPROC proc)
{
    struct wnd *w = W(h);
    int free_ = -1;
    if (!w)
        id = 0;
    for (int i = 0; i < NTIMERS; i++) {
        if (timers[i].used && timers[i].w == w && w && timers[i].id == id) {
            free_ = i;
            break;
        }
        if (!timers[i].used && free_ < 0)
            free_ = i;
    }
    if (free_ < 0)
        return 0;
    struct timer *t = &timers[free_];
    if (!w)
        id = (UINT)(free_ + 1);
    t->used = 1;
    t->w = w;
    t->id = id;
    t->proc = proc;
    t->every = (uint16_t)((ms + 54) / 55);
    if (!t->every)
        t->every = 1;
    t->next = (uint16_t)(cw_ticks() + t->every);
    return id ? id : 1;
}

BOOL
KillTimer(HWND h, UINT id)
{
    struct wnd *w = W(h);
    for (int i = 0; i < NTIMERS; i++)
        if (timers[i].used && timers[i].w == w && timers[i].id == id) {
            timers[i].used = 0;
            return TRUE;
        }
    return FALSE;
}

static void
killTimers(struct wnd *w)
{
    for (int i = 0; i < NTIMERS; i++)
        if (timers[i].used && timers[i].w == w)
            timers[i].used = 0;
}

/* ---- The message queue. ---- */

#define QLEN 64
static MSG queue[QLEN];
static int qhead, qcount;
static BYTE quitting;
static int quitCode;

static void
push(const MSG *m, int front)
{
    if (qcount == QLEN)
        return;
    if (front) {
        qhead = (qhead + QLEN - 1) % QLEN;
        queue[qhead] = *m;
    } else {
        queue[(qhead + qcount) % QLEN] = *m;
    }
    qcount++;
}

static void
post(HWND h, UINT msg, WPARAM wp, LPARAM lp, int front)
{
    MSG m;
    m.hwnd = h;
    m.message = msg;
    m.wParam = wp;
    m.lParam = lp;
    m.time = cw_ticks();
    m.pt.x = cw_mx;
    m.pt.y = cw_my;
    push(&m, front);
}

void cw_post(struct wnd *w, UINT msg, WPARAM wp, LPARAM lp) { if (w) post(w->hwnd, msg, wp, lp, 0); }

BOOL
PostMessage(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    cw_setup();
    if (h && !W(h))
        return FALSE;
    if (qcount == QLEN)
        return FALSE;
    post(h, msg, wp, lp, 0);
    return TRUE;
}

void
PostQuitMessage(int code)
{
    quitting = 1;
    quitCode = code;
}

LRESULT
cw_send(struct wnd *w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (!w || (w->destroyed && msg != WM_DESTROY))
        return 0;
    return w->proc(w->hwnd, msg, wp, lp);
}

LRESULT SendMessage(HWND h, UINT msg, WPARAM wp, LPARAM lp) { return cw_send(W(h), msg, wp, lp); }

LRESULT
CallWindowProc(WNDPROC proc, HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    return proc ? proc(h, msg, wp, lp) : 0;
}

static void
runTimers(void)
{
    uint16_t now = cw_ticks();
    for (int i = 0; i < NTIMERS; i++) {
        struct timer *t = &timers[i];
        if (!t->used || (int16_t)(now - t->next) < 0)
            continue;
        t->next = (uint16_t)(now + t->every);
        /* One WM_TIMER at a time for each timer, as in Windows. */
        int queued = 0;
        for (int k = 0; k < qcount; k++) {
            MSG *m = &queue[(qhead + k) % QLEN];
            if (m->message == WM_TIMER && m->wParam == t->id && m->lParam == i + 1)
                queued = 1;
        }
        if (!queued)
            post(t->w ? t->w->hwnd : 0, WM_TIMER, t->id, i + 1, 0);
    }
}

/* One step of getting a message: TRUE with *m filled, or FALSE if there
 * is nothing now (after painting). */
static BOOL
step(LPMSG m, int remove)
{
    for (;;) {
        if (qcount) {
            *m = queue[qhead];
            if (remove) {
                qhead = (qhead + 1) % QLEN;
                qcount--;
            }
            msgTime = m->time;
            if (m->hwnd && !W(m->hwnd)) {
                if (!remove) {
                    qhead = (qhead + 1) % QLEN;
                    qcount--;
                }
                continue;
            }
            return TRUE;
        }
        if (quitting) {
            m->hwnd = 0;
            m->message = WM_QUIT;
            m->wParam = (WPARAM)quitCode;
            m->lParam = 0;
            if (remove)
                quitting = 0;
            return TRUE;
        }
        runTimers();
        if (qcount)
            continue;
        show(1);
        struct cwev e;
        if (cw_poll(&e)) {
            cw_dispatch_input(&e);
            continue;
        }
        return FALSE;
    }
}

BOOL
GetMessage(LPMSG m, HWND h, UINT first, UINT last)
{
    (void)h;
    (void)first;
    (void)last;
    cw_setup();
    while (!step(m, 1))
        cw_idle();
    return m->message != WM_QUIT;
}

BOOL
PeekMessage(LPMSG m, HWND h, UINT first, UINT last, UINT flags)
{
    (void)h;
    (void)first;
    (void)last;
    cw_setup();
    return step(m, flags & PM_REMOVE);
}

BOOL
WaitMessage(void)
{
    MSG m;
    cw_setup();
    while (!step(&m, 0))
        cw_idle();
    return TRUE;
}

DWORD GetMessageTime(void) { return msgTime; }

DWORD
GetTickCount(void)
{
    DWORD t = *(volatile DWORD __far *)(((uint32_t)0x40 << 16) | 0x6c);
    return t * 55;
}

LRESULT
DispatchMessage(const MSG *m)
{
    if (m->message == WM_TIMER && m->lParam >= 1 && m->lParam <= NTIMERS) {
        struct timer *t = &timers[m->lParam - 1];
        if (t->used && t->proc) {
            t->proc(m->hwnd, WM_TIMER, t->id, GetTickCount());
            return 0;
        }
    }
    return SendMessage(m->hwnd, m->message, m->wParam, m->lParam);
}

BOOL
TranslateMessage(const MSG *m)
{
    if (m->message != WM_KEYDOWN && m->message != WM_SYSKEYDOWN)
        return FALSE;
    if ((BYTE)HIWORD(m->lParam) != (BYTE)(lastKey >> 8))
        return FALSE;
    int ch = lastKey & 0xff;
    if (m->message == WM_SYSKEYDOWN) {
        int vk = (int)m->wParam;
        if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
            post(m->hwnd, WM_SYSCHAR, vk >= 'A' ? vk + 32 : vk, m->lParam, 1);
            return TRUE;
        }
        return FALSE;
    }
    if (!ch)
        return FALSE;
    post(m->hwnd, WM_CHAR, (WPARAM)ch, m->lParam, 1);
    return TRUE;
}

int
GetKeyState(int vk)
{
    BYTE s = cw_shiftflags();
    int down = 0;
    switch (vk) {
    case VK_SHIFT:
        down = s & 3;
        break;
    case VK_CONTROL:
        down = s & 4;
        break;
    case VK_MENU:
        down = s & 8;
        break;
    case 0x14: /* VK_CAPITAL: the toggle */
        return (s & 0x40) ? 1 : 0;
    case 0x90: /* VK_NUMLOCK */
        return (s & 0x20) ? 1 : 0;
    }
    return down ? (int)0x8000 : 0;
}

/* ---- Accelerators. ---- */

HACCEL
CwCreateAcceleratorTable(const ACCEL *t, int n)
{
    cw_setup();
    if (n <= 0)
        return 0;
    struct accel *a = (struct accel *)malloc(sizeof *a + (n - 1) * sizeof(ACCEL));
    if (!a)
        return 0;
    a->kind = OBJ_ACCEL;
    a->n = n;
    memcpy(a->t, t, n * sizeof(ACCEL));
    a->hacc = cw_newhandle(a);
    if (!a->hacc) {
        free(a);
        return 0;
    }
    return a->hacc;
}

BOOL
DestroyAcceleratorTable(HACCEL h)
{
    struct accel *a = (struct accel *)cw_obj(h, OBJ_ACCEL);
    if (!a)
        return FALSE;
    cw_freehandle(h);
    free(a);
    return TRUE;
}

int
TranslateAccelerator(HWND h, HACCEL acc, LPMSG m)
{
    struct accel *a = (struct accel *)cw_obj(acc, OBJ_ACCEL);
    struct wnd *w = W(h);
    if (!a || !w)
        return 0;
    if (m->message != WM_KEYDOWN && m->message != WM_SYSKEYDOWN && m->message != WM_CHAR &&
        m->message != WM_SYSCHAR)
        return 0;
    BYTE s = cw_shiftflags();
    BYTE mods = (BYTE)(((s & 3) ? FSHIFT : 0) | ((s & 4) ? FCONTROL : 0) | ((s & 8) ? FALT : 0));
    for (int i = 0; i < a->n; i++) {
        const ACCEL *e = &a->t[i];
        int match;
        if (e->fVirt & FVIRTKEY)
            match = (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN) &&
                    m->wParam == e->key && (e->fVirt & (FSHIFT | FCONTROL | FALT)) == mods;
        else
            match = (m->message == WM_CHAR || m->message == WM_SYSCHAR) && m->wParam == e->key;
        if (!match)
            continue;
        if (!(w->style & WS_DISABLED))
            cw_send(w, WM_COMMAND, e->cmd, MAKELONG(0, 1));
        return 1;
    }
    return 0;
}

/* ---- Input. ---- */

/* Virtual keys of the BIOS scan codes that have no character. */
int
cw_vkey(WORD key)
{
    int ch = key & 0xff, sc = key >> 8;
    if (ch >= 'a' && ch <= 'z')
        return ch - 32;
    if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
        return ch;
    if (sc >= 0x3b && sc <= 0x44)
        return VK_F1 + sc - 0x3b;
    if (sc >= 0x54 && sc <= 0x5d)
        return VK_F1 + sc - 0x54; /* shift */
    if (sc >= 0x5e && sc <= 0x67)
        return VK_F1 + sc - 0x5e; /* ctrl */
    if (sc >= 0x68 && sc <= 0x71)
        return VK_F1 + sc - 0x68; /* alt */
    if (sc >= 0x78 && sc <= 0x80)
        return '1' + sc - 0x78; /* alt digits */
    static const BYTE letters[] = "QWERTYUIOP\0\0\0\0ASDFGHJKL\0\0\0\0\0ZXCVBNM";
    if (sc >= 0x10 && sc <= 0x32 && letters[sc - 0x10])
        return letters[sc - 0x10];
    switch (sc) {
    case 0x01: return VK_ESCAPE;
    case 0x0e: return VK_BACK;
    case 0x0f: case 0x94: case 0xa5: return VK_TAB;
    case 0x1c: case 0xa6: return VK_RETURN;
    case 0x39: return VK_SPACE;
    case 0x81: return '0';
    case 0x47: case 0x77: case 0x97: return VK_HOME;
    case 0x48: case 0x8d: case 0x98: return VK_UP;
    case 0x49: case 0x84: case 0x99: return VK_PRIOR;
    case 0x4a: return VK_SUBTRACT;
    case 0x4b: case 0x73: case 0x9b: return VK_LEFT;
    case 0x4d: case 0x74: case 0x9d: return VK_RIGHT;
    case 0x4e: return VK_ADD;
    case 0x37: return VK_MULTIPLY;
    case 0x4f: case 0x75: case 0x9f: return VK_END;
    case 0x50: case 0x91: case 0xa0: return VK_DOWN;
    case 0x51: case 0x76: case 0xa1: return VK_NEXT;
    case 0x52: case 0x92: case 0xa2: return VK_INSERT;
    case 0x53: case 0x93: case 0xa3: return VK_DELETE;
    case 0x85: case 0x87: case 0x89: case 0x8b: return VK_F11;
    case 0x86: case 0x88: case 0x8a: case 0x8c: return VK_F12;
    }
    return ch ? ch : 0xff;
}

static void ncdown(struct wnd *w, const struct cwev *e);

void
cw_dispatch_input(const struct cwev *e)
{
    if (e->type == EV_KEY || e->type == EV_ALTTAP) {
        struct wnd *t = cw_focus ? cw_focus : cw_active;
        if (!t)
            return;
        if (e->type == EV_ALTTAP) {
            post(t->hwnd, WM_SYSKEYUP, VK_MENU, MAKELONG(1, 0xc038), 0);
            return;
        }
        lastKey = e->key;
        int vk = cw_vkey(e->key);
        int alt = (e->shift & 8) != 0;
        WORD hi = (WORD)((e->key >> 8) | (alt ? 0x2000 : 0));
        post(t->hwnd, (alt || vk == VK_F10) ? WM_SYSKEYDOWN : WM_KEYDOWN, vk, MAKELONG(1, hi), 0);
        return;
    }
    if (e->type == EV_NONE)
        return;
    struct wnd *t = cw_capture;
    if (!t) {
        t = hit(cw_desktop, e->x, e->y);
        if (e->type == EV_DOWN) {
            struct wnd *top = cw_top(t);
            if (top && top != cw_desktop) {
                if (top->style & WS_DISABLED) {
                    MessageBeep(0);
                    return;
                }
                if (top != cw_active)
                    SetActiveWindow(top->hwnd);
            }
            RECT c;
            cw_clientrect(t, &c);
            if (t != cw_desktop && !inRect(&c, e->x, e->y)) {
                ncdown(t, e);
                return;
            }
        }
        if (t == cw_desktop || (t->style & WS_DISABLED))
            return;
        for (struct wnd *p = t; p && p != cw_desktop; p = p->parent)
            if (p->style & WS_DISABLED)
                return;
    }
    RECT c;
    cw_clientrect(t, &c);
    BYTE s = e->shift;
    WPARAM mk = (WPARAM)(((s & 3) ? MK_SHIFT : 0) | ((s & 4) ? MK_CONTROL : 0));
    BYTE b = e->type == EV_UP ? 0 : e->buttons;
    mk |= (b & 1 ? MK_LBUTTON : 0) | (b & 2 ? MK_RBUTTON : 0);
    LPARAM lp = MAKELONG(e->x - c.left, e->y - c.top);
    UINT msg;
    switch (e->type) {
    case EV_DOWN:
        if (e->buttons & 1)
            msg = e->dbl && (t->cls->wc.style & CS_DBLCLKS) ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN;
        else
            msg = WM_RBUTTONDOWN;
        break;
    case EV_UP:
        msg = (e->buttons & 1) ? WM_LBUTTONUP : WM_RBUTTONUP;
        break;
    default:
        msg = WM_MOUSEMOVE;
    }
    post(t->hwnd, msg, mk, lp, 0);
}

void
cw_wait_input(struct cwev *e)
{
    for (;;) {
        show(0);
        if (cw_poll(e))
            return;
        cw_idle();
    }
}

/* Clicks on scroll bars: SB_ codes, repeated while the button is down. */
static void
scrollClick(struct wnd *w, int y)
{
    RECT s;
    cw_scrollbar_rect(w, &s);
    int len = s.bottom - s.top;
    int thumb = s.top + 1 + thumbRow(w, len - 2);
    HWND h = w->hwnd;
    if (len >= 3 && y == thumb) {
        struct cwev e;
        int pos = w->sbpos;
        for (;;) {
            cw_wait_input(&e);
            if (!W(h))
                return;
            if (e.type == EV_MOVE || e.type == EV_UP) {
                int r = cw_imax(0, cw_imin(len - 3, e.y - s.top - 1));
                pos = len > 3 ? w->sbmin + (int)((long)r * (w->sbmax - w->sbmin) / (len - 3)) : w->sbmin;
                cw_send(w, WM_VSCROLL, e.type == EV_UP ? SB_THUMBPOSITION : SB_THUMBTRACK, MAKELONG(pos, 0));
            }
            if (e.type == EV_UP)
                break;
        }
        cw_send(w, WM_VSCROLL, SB_ENDSCROLL, 0);
        return;
    }
    int code = y == s.top ? SB_LINEUP : y == s.bottom - 1 ? SB_LINEDOWN : y < thumb ? SB_PAGEUP : SB_PAGEDOWN;
    cw_send(w, WM_VSCROLL, code, 0);
    uint16_t start = cw_ticks(), last = start;
    for (;;) {
        show(0);
        struct cwev e;
        if (cw_poll(&e) && e.type == EV_UP)
            break;
        uint16_t now = cw_ticks();
        if ((uint16_t)(now - start) >= 6 && now != last) {
            last = now;
            if (!W(h))
                return;
            cw_send(w, WM_VSCROLL, code, 0);
        }
        cw_idle();
    }
    if (W(h))
        cw_send(w, WM_VSCROLL, SB_ENDSCROLL, 0);
}

/* Dragging a window by its title, or sizing it by its corner. */
static void
drag(struct wnd *w, const struct cwev *start, int size)
{
    int ox = w->x, oy = w->y, ow = w->w, oh = w->h;
    HWND h = w->hwnd;
    struct cwev e;
    for (;;) {
        cw_wait_input(&e);
        if (!W(h))
            return;
        if (e.type == EV_KEY && (e.key & 0xff) == 27) {
            MoveWindow(h, ox, oy, ow, oh, TRUE);
            return;
        }
        if (e.type == EV_MOVE || e.type == EV_UP) {
            int dx = e.x - start->x, dy = e.y - start->y;
            if (size)
                MoveWindow(h, ox, oy, cw_imax(10, ow + dx), cw_imax(4, oh + dy), TRUE);
            else
                MoveWindow(h, ox + dx, cw_imax(0, oy + dy), ow, oh, TRUE);
        }
        if (e.type == EV_UP)
            return;
    }
}

static void
ncdown(struct wnd *w, const struct cwev *e)
{
    RECT wr;
    cw_winrect(w, &wr);
    if (isTop(w) && w->menu && e->y == cw_menubar_row(w)) {
        int item = cw_menubar_hit(w, e->x, e->y);
        if (item >= 0)
            cw_track_bar(w, item, TRUE, TRUE);
        return;
    }
    if (w->style & WS_VSCROLL) {
        RECT s;
        cw_scrollbar_rect(w, &s);
        if (inRect(&s, e->x, e->y)) {
            scrollClick(w, e->y);
            return;
        }
    }
    if (!isTop(w) || !(e->buttons & 1))
        return;
    int caption = (w->style & WS_CAPTION) == WS_CAPTION;
    if (caption && e->y == wr.top) {
        int boxes = !(w->style & WS_POPUP) && w->w >= 10;
        if (boxes && (w->style & WS_SYSMENU) && e->x >= wr.left + 1 && e->x <= wr.left + 3) {
            cw_post(w, WM_SYSCOMMAND, SC_CLOSE, 0);
            return;
        }
        if (boxes && (w->style & WS_MAXIMIZEBOX) &&
            ((e->x >= wr.right - 4 && e->x <= wr.right - 2) || e->dbl)) {
            cw_post(w, WM_SYSCOMMAND, w->zoomed ? SC_RESTORE : SC_MAXIMIZE, 0);
            return;
        }
        if (!w->zoomed)
            drag(w, e, 0);
        return;
    }
    if ((w->style & WS_THICKFRAME) && !w->zoomed && e->x == wr.right - 1 && e->y == wr.bottom - 1)
        drag(w, e, 1);
}

/* ---- The status line. ---- */

void
CwSetStatusText(LPCSTR text)
{
    cw_setup();
    free(cw_status);
    cw_status = text ? cw_strdup(text) : 0;
    int h = text ? cw_rows - 1 : cw_rows;
    if (cw_desktop->h != h) {
        cw_desktop->h = h;
        for (struct wnd *c = cw_desktop->first; c; c = c->next)
            if (c->zoomed)
                MoveWindow(c->hwnd, 0, 0, cw_cols, h, TRUE);
    }
    cw_dirty = 1;
}

/* ---- DefWindowProc. ---- */

LRESULT
DefWindowProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    struct wnd *w = W(h);
    if (!w)
        return 0;
    switch (msg) {
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_SETTEXT: {
        char *t = cw_strdup((const char *)PTR(lp));
        if (!t)
            return FALSE;
        free(w->text);
        w->text = t;
        cw_dirty = 1;
        return TRUE;
    }
    case WM_GETTEXT: {
        char *buf = (char *)PTR(lp);
        int max = (int)wp;
        if (max <= 0 || !buf)
            return 0;
        strncpy(buf, w->text, max - 1);
        buf[max - 1] = 0;
        return (LRESULT)strlen(buf);
    }
    case WM_GETTEXTLENGTH:
        return (LRESULT)strlen(w->text);
    case WM_ERASEBKGND: {
        int a = brushAttr(w->cls->wc.hbrBackground);
        if (w->isDialog)
            a = cw_sys[COLOR_DIALOG];
        if (a < 0)
            return 0;
        cw_fill(&w->dcclip, w->dcclip.left, w->dcclip.top, w->dcclip.right - w->dcclip.left,
                w->dcclip.bottom - w->dcclip.top, ' ', (BYTE)a);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_SYSCOMMAND:
        switch (wp & 0xfff0) {
        case SC_CLOSE:
            cw_send(w, WM_CLOSE, 0, 0);
            break;
        case SC_MAXIMIZE:
            ShowWindow(h, SW_MAXIMIZE);
            break;
        case SC_RESTORE:
            ShowWindow(h, SW_RESTORE);
            break;
        }
        return 0;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_SYSCHAR:
        if ((w->style & WS_CHILD) && w->parent)
            return cw_send(w->parent, msg, wp, lp);
        if (w->style & WS_DISABLED)
            return 0;
        if (msg == WM_SYSKEYDOWN && wp == VK_F4 && (HIWORD(lp) & 0x2000)) {
            cw_post(w, WM_SYSCOMMAND, SC_CLOSE, 0);
            return 0;
        }
        if (!w->menu) {
            if (msg == WM_SYSCHAR)
                MessageBeep(0);
            return 0;
        }
        if (msg == WM_SYSKEYDOWN && wp == VK_F10)
            cw_track_bar(w, 0, FALSE, FALSE);
        else if (msg == WM_SYSKEYUP && wp == VK_MENU)
            cw_track_bar(w, 0, FALSE, FALSE);
        else if (msg == WM_SYSCHAR) {
            int item = cw_menu_findkey(w, cw_upper((int)wp));
            if (item >= 0)
                cw_track_bar(w, item, TRUE, FALSE);
            else
                MessageBeep(0);
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE && (!cw_focus || cw_top(cw_focus) != w)) {
            struct wnd *f = w->lastFocus;
            if (f && within(f, w) && visible(f) && !(f->style & WS_DISABLED))
                SetFocus(f->hwnd);
            else
                SetFocus(h);
        }
        return 0;
    }
    return 0;
}
