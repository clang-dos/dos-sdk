/* Menus: the menu bar on a top-level window, pull-down and popup menus,
 * and the modal loop that runs them with the keyboard and the mouse.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "cwin-local.h"
#include <stdlib.h>
#include <string.h>

/* ---- Menu objects. ---- */

static HMENU
newMenu(int popup)
{
    cw_setup();
    struct menu *m = (struct menu *)calloc(1, sizeof *m);
    if (!m)
        return 0;
    m->kind = OBJ_MENU;
    m->popup = (BYTE)popup;
    m->hmenu = cw_newhandle(m);
    if (!m->hmenu) {
        free(m);
        return 0;
    }
    return m->hmenu;
}

HMENU CreateMenu(void) { return newMenu(0); }
HMENU CreatePopupMenu(void) { return newMenu(1); }

static void
freeItem(struct menuitem *it)
{
    if (it->flags & MF_POPUP)
        DestroyMenu((HMENU)it->id);
    free(it->text);
}

BOOL
DestroyMenu(HMENU h)
{
    struct menu *m = M(h);
    if (!m)
        return FALSE;
    cw_freehandle(h);
    for (int i = 0; i < m->n; i++)
        freeItem(&m->items[i]);
    free(m->items);
    free(m);
    return TRUE;
}

static int
setItem(struct menuitem *it, UINT flags, UINT id, LPCSTR text)
{
    it->flags = flags & ~(MF_BYPOSITION | MF_APPEND | MF_CHANGE | MF_DELETE | MF_REMOVE);
    it->id = id;
    it->text = (flags & MF_SEPARATOR) ? 0 : cw_strdup(text);
    return (flags & MF_SEPARATOR) || it->text;
}

BOOL
AppendMenu(HMENU h, UINT flags, UINT id, LPCSTR text)
{
    struct menu *m = M(h);
    if (!m)
        return FALSE;
    if (m->n == m->cap) {
        int n = m->cap ? m->cap * 2 : 8;
        struct menuitem *t = (struct menuitem *)realloc(m->items, n * sizeof *t);
        if (!t)
            return FALSE;
        m->items = t;
        m->cap = n;
    }
    if (!setItem(&m->items[m->n], flags, id, text))
        return FALSE;
    m->n++;
    cw_dirty = 1;
    return TRUE;
}

/* The item by command (in submenus too) or by position. */
static struct menuitem *
findItem(struct menu *m, UINT item, UINT flags, struct menu **owner, int *pos)
{
    if (!m)
        return 0;
    if (flags & MF_BYPOSITION) {
        if (item >= (UINT)m->n)
            return 0;
        if (owner)
            *owner = m;
        if (pos)
            *pos = (int)item;
        return &m->items[item];
    }
    for (int i = 0; i < m->n; i++) {
        struct menuitem *it = &m->items[i];
        if (it->flags & MF_POPUP) {
            struct menuitem *r = findItem(M((HMENU)it->id), item, flags, owner, pos);
            if (r)
                return r;
        } else if (!(it->flags & MF_SEPARATOR) && it->id == item) {
            if (owner)
                *owner = m;
            if (pos)
                *pos = i;
            return it;
        }
    }
    return 0;
}

BOOL
ModifyMenu(HMENU h, UINT item, UINT flags, UINT id, LPCSTR text)
{
    struct menuitem *it = findItem(M(h), item, flags, 0, 0);
    if (!it)
        return FALSE;
    char *old = it->text;
    UINT oldFlags = it->flags, oldId = it->id;
    if (!setItem(it, flags, id, text))
        return FALSE;
    free(old);
    if ((oldFlags & MF_POPUP) && !((it->flags & MF_POPUP) && it->id == oldId))
        DestroyMenu((HMENU)oldId);
    cw_dirty = 1;
    return TRUE;
}

BOOL
DeleteMenu(HMENU h, UINT item, UINT flags)
{
    struct menu *m;
    int pos;
    struct menuitem *it = findItem(M(h), item, flags, &m, &pos);
    if (!it)
        return FALSE;
    freeItem(it);
    memmove(&m->items[pos], &m->items[pos + 1], (m->n - pos - 1) * sizeof *it);
    m->n--;
    cw_dirty = 1;
    return TRUE;
}

UINT
CheckMenuItem(HMENU h, UINT item, UINT flags)
{
    struct menuitem *it = findItem(M(h), item, flags, 0, 0);
    if (!it)
        return (UINT)-1;
    UINT old = it->flags & MF_CHECKED;
    it->flags = (it->flags & ~MF_CHECKED) | (flags & MF_CHECKED);
    cw_dirty = 1;
    return old;
}

BOOL
EnableMenuItem(HMENU h, UINT item, UINT flags)
{
    struct menuitem *it = findItem(M(h), item, flags, 0, 0);
    if (!it)
        return -1;
    UINT old = it->flags & (MF_GRAYED | MF_DISABLED);
    it->flags = (it->flags & ~(MF_GRAYED | MF_DISABLED)) | (flags & (MF_GRAYED | MF_DISABLED));
    cw_dirty = 1;
    return (BOOL)old;
}

UINT
GetMenuState(HMENU h, UINT item, UINT flags)
{
    struct menuitem *it = findItem(M(h), item, flags, 0, 0);
    return it ? it->flags : (UINT)-1;
}

int
GetMenuString(HMENU h, UINT item, LPSTR buf, int max, UINT flags)
{
    struct menuitem *it = findItem(M(h), item, flags, 0, 0);
    if (!it || max <= 0)
        return 0;
    strncpy(buf, it->text ? it->text : "", max - 1);
    buf[max - 1] = 0;
    return (int)strlen(buf);
}

int GetMenuItemCount(HMENU h) { struct menu *m = M(h); return m ? m->n : -1; }

HMENU
GetSubMenu(HMENU h, int pos)
{
    struct menu *m = M(h);
    if (!m || pos < 0 || pos >= m->n || !(m->items[pos].flags & MF_POPUP))
        return 0;
    return (HMENU)m->items[pos].id;
}

UINT
GetMenuItemID(HMENU h, int pos)
{
    struct menu *m = M(h);
    if (!m || pos < 0 || pos >= m->n || (m->items[pos].flags & MF_POPUP))
        return (UINT)-1;
    return m->items[pos].id;
}

BOOL
SetMenu(HWND h, HMENU menu)
{
    struct wnd *w = W(h);
    if (!w || (w->style & WS_CHILD))
        return FALSE;
    if (menu && !M(menu))
        return FALSE;
    w->menu = menu;
    RECT in;
    cw_clientinset(w, &in);
    cw_send(w, WM_SIZE, SIZE_RESTORED, MAKELONG(in.right - in.left, in.bottom - in.top));
    cw_dirty = 1;
    return TRUE;
}

HMENU GetMenu(HWND h) { struct wnd *w = W(h); return w && !(w->style & WS_CHILD) ? w->menu : 0; }
void DrawMenuBar(HWND h) { (void)h; cw_dirty = 1; }

/* ---- Tracking state. ---- */

#define MAXDEPTH 4
static struct {
    BYTE active;
    struct wnd *owner;
    struct menu *bar; /* or 0 for TrackPopupMenu */
    struct wnd *barwnd;
    int barSel;
    int depth;
    struct {
        struct menu *m;
        int x, y, w, h, sel;
    } pop[MAXDEPTH];
} mt;

/* ---- The menu bar. ---- */

int
cw_menubar_row(struct wnd *w)
{
    if (!w->menu)
        return -1;
    RECT wr;
    cw_winrect(w, &wr);
    return wr.top + (cw_hasframe(w) ? 1 : 0);
}

/* The screen column and width of bar item i. */
static int
barItem(struct wnd *w, struct menu *m, int i, int *x)
{
    RECT wr;
    cw_winrect(w, &wr);
    int f = cw_hasframe(w) ? 1 : 0;
    int left = wr.left + f + 1, right = wr.right - f - 1;
    int px = left;
    for (int k = 0; k < m->n; k++) {
        struct menuitem *it = &m->items[k];
        int wd = cw_textlen(it->text) + 2;
        if (it->flags & MF_HELP) {
            /* This and the rest go at the right end. */
            int rest = 0;
            for (int j = k; j < m->n; j++)
                rest += cw_textlen(m->items[j].text) + 2;
            if (right - rest > px)
                px = right - rest;
        }
        if (k == i) {
            *x = px;
            return wd;
        }
        px += wd;
    }
    *x = px;
    return 0;
}

static BYTE
itemAttr(UINT flags, int sel, BYTE *hot)
{
    BYTE a = cw_sys[sel ? COLOR_MENUSEL : COLOR_MENU];
    *hot = cw_sys[sel ? COLOR_MENUSELHOT : COLOR_MENUHOT];
    if (flags & (MF_GRAYED | MF_DISABLED)) {
        a = sel ? (BYTE)((a & 0xf0) | 0x08) : cw_sys[COLOR_GRAYTEXT];
        *hot = a;
    }
    return a;
}

void
cw_menubar_paint(struct wnd *w, const RECT *clip)
{
    struct menu *m = M(w->menu);
    if (!m)
        return;
    RECT wr;
    cw_winrect(w, &wr);
    int f = cw_hasframe(w) ? 1 : 0;
    int y = cw_menubar_row(w);
    cw_fill(clip, wr.left + f, y, w->w - 2 * f, 1, ' ', cw_sys[COLOR_MENU]);
    for (int i = 0; i < m->n; i++) {
        int x, wd = barItem(w, m, i, &x);
        int sel = mt.active && mt.barwnd == w && mt.barSel == i;
        BYTE hot, a = itemAttr(m->items[i].flags, sel, &hot);
        cw_fill(clip, x, y, wd, 1, ' ', a);
        cw_text(clip, x + 1, y, m->items[i].text, -1, a, hot);
    }
}

int
cw_menubar_hit(struct wnd *w, int x, int y)
{
    struct menu *m = M(w->menu);
    if (!m || y != cw_menubar_row(w))
        return -1;
    for (int i = 0; i < m->n; i++) {
        int ix, wd = barItem(w, m, i, &ix);
        if (x >= ix && x < ix + wd)
            return i;
    }
    return -1;
}

static int
findKey(struct menu *m, int c)
{
    for (int i = 0; m && i < m->n; i++)
        if (m->items[i].text && cw_mnemonic(m->items[i].text) == c)
            return i;
    return -1;
}

int cw_menu_findkey(struct wnd *w, int c) { return findKey(M(w->menu), c); }

/* ---- Popups. ---- */

static void
popupSize(struct menu *m, int *w, int *h)
{
    int text = 0, acc = 0;
    for (int i = 0; i < m->n; i++) {
        const char *t = m->items[i].text;
        if (!t)
            continue;
        text = cw_imax(text, cw_textlen(t));
        const char *tab = strchr(t, '\t');
        if (tab)
            acc = cw_imax(acc, (int)strlen(tab + 1));
    }
    *w = text + (acc ? acc + 3 : 0) + 4;
    if (m->popup == 0 || 1)
        *w += 0;
    *h = m->n + 2;
}

void
cw_menu_overlay(void)
{
    if (!mt.active)
        return;
    RECT all;
    cw_rect(&all, 0, 0, cw_cols, cw_rows);
    for (int d = 0; d < mt.depth; d++) {
        struct menu *m = mt.pop[d].m;
        int x = mt.pop[d].x, y = mt.pop[d].y, w = mt.pop[d].w, h = mt.pop[d].h;
        BYTE a = cw_sys[COLOR_MENU];
        cw_shade(&all, x + w, y + 1, 2, h);
        cw_shade(&all, x + 2, y + h, w, 1);
        cw_fill(&all, x, y, w, h, ' ', a);
        cw_box(&all, x, y, w, h, a, FALSE);
        for (int i = 0; i < m->n; i++) {
            struct menuitem *it = &m->items[i];
            int iy = y + 1 + i;
            if (it->flags & MF_SEPARATOR) {
                cw_put(&all, x, iy, 0xc3, a);
                cw_fill(&all, x + 1, iy, w - 2, 1, 0xc4, a);
                cw_put(&all, x + w - 1, iy, 0xb4, a);
                continue;
            }
            int sel = mt.pop[d].sel == i;
            BYTE hot, ia = itemAttr(it->flags, sel, &hot);
            cw_fill(&all, x + 1, iy, w - 2, 1, ' ', ia);
            if (it->flags & MF_CHECKED)
                cw_put(&all, x + 1, iy, 0xfb, ia);
            cw_text(&all, x + 2, iy, it->text, -1, ia, hot);
            const char *tab = strchr(it->text, '\t');
            if (tab) {
                int n = (int)strlen(tab + 1);
                cw_text(&all, x + w - 2 - n, iy, tab + 1, -1, ia, 0);
            }
            if (it->flags & MF_POPUP)
                cw_put(&all, x + w - 2, iy, 0x10, ia);
        }
    }
}

static int
selectable(struct menuitem *it)
{
    return !(it->flags & MF_SEPARATOR);
}

static void
menuSelect(void)
{
    struct menu *m;
    int i;
    if (mt.depth > 0 && mt.pop[mt.depth - 1].sel >= 0) {
        m = mt.pop[mt.depth - 1].m;
        i = mt.pop[mt.depth - 1].sel;
    } else if (mt.bar && mt.barSel >= 0) {
        m = mt.bar;
        i = mt.barSel;
    } else {
        return;
    }
    struct menuitem *it = &m->items[i];
    UINT id = (it->flags & MF_POPUP) ? (UINT)i : it->id;
    cw_send(mt.owner, WM_MENUSELECT, id, MAKELONG(it->flags, m->hmenu));
    cw_dirty = 1;
}

/* Open popup m as level d, below or beside x, y. */
static void
openPopup(int d, struct menu *m, int x, int y, int index, int kbd)
{
    if (!m || d >= MAXDEPTH)
        return;
    HMENU hm = m->hmenu;
    cw_send(mt.owner, WM_INITMENUPOPUP, hm, MAKELONG(index, 0));
    if (!M(hm))
        return;
    int w, h;
    popupSize(m, &w, &h);
    if (x + w + 2 > cw_cols)
        x = cw_imax(0, cw_cols - w - 2);
    if (y + h > cw_rows)
        y = cw_imax(0, cw_rows - h);
    mt.pop[d].m = m;
    mt.pop[d].x = x;
    mt.pop[d].y = y;
    mt.pop[d].w = w;
    mt.pop[d].h = h;
    mt.pop[d].sel = -1;
    if (kbd)
        for (int i = 0; i < m->n; i++)
            if (selectable(&m->items[i])) {
                mt.pop[d].sel = i;
                break;
            }
    mt.depth = d + 1;
    cw_dirty = 1;
    menuSelect();
}

static void
openBar(int item, int kbd)
{
    mt.depth = 0;
    mt.barSel = item;
    cw_dirty = 1;
    if (item < 0 || !mt.bar)
        return;
    struct menuitem *it = &mt.bar->items[item];
    if (it->flags & MF_POPUP) {
        int x;
        barItem(mt.barwnd, mt.bar, item, &x);
        openPopup(0, M((HMENU)it->id), x - 1, cw_menubar_row(mt.barwnd) + 1, item, kbd);
    } else {
        menuSelect();
    }
}

static void
openSub(int kbd)
{
    int d = mt.depth - 1;
    struct menuitem *it = &mt.pop[d].m->items[mt.pop[d].sel];
    if (it->flags & MF_POPUP)
        openPopup(d + 1, M((HMENU)it->id), mt.pop[d].x + mt.pop[d].w - 1,
                  mt.pop[d].y + mt.pop[d].sel, mt.pop[d].sel, kbd);
}

static void
moveSel(int d, int dir)
{
    struct menu *m = mt.pop[d].m;
    int i = mt.pop[d].sel;
    for (int k = 0; k < m->n; k++) {
        i = (i + dir + m->n) % m->n;
        if (selectable(&m->items[i]))
            break;
    }
    mt.pop[d].sel = i;
    mt.depth = d + 1;
    menuSelect();
}

/* Choose the item: 0 to go on, 1 to end with *id (0 for nothing). */
static int
choose(struct menu *m, int i, UINT *id, int kbd)
{
    struct menuitem *it = &m->items[i];
    if (it->flags & MF_SEPARATOR)
        return 0;
    if (it->flags & MF_POPUP) {
        if (m == mt.bar)
            openBar(i, kbd);
        else {
            for (int d = 0; d < mt.depth; d++)
                if (mt.pop[d].m == m) {
                    mt.pop[d].sel = i;
                    mt.depth = d + 1;
                }
            openSub(kbd);
        }
        return 0;
    }
    if (it->flags & (MF_GRAYED | MF_DISABLED)) {
        MessageBeep(0);
        return 0;
    }
    *id = it->id;
    return 1;
}

/* The popup level and item, or the bar item (*d = -1), at a cell. */
static int
menuHit(int x, int y, int *d)
{
    for (int k = mt.depth - 1; k >= 0; k--) {
        int px = mt.pop[k].x, py = mt.pop[k].y;
        if (x >= px && x < px + mt.pop[k].w && y >= py && y < py + mt.pop[k].h) {
            *d = k;
            int r = y - py - 1;
            return x > px && x < px + mt.pop[k].w - 1 && r >= 0 && r < mt.pop[k].m->n ? r : -1;
        }
    }
    *d = -2;
    if (mt.bar) {
        int i = cw_menubar_hit(mt.barwnd, x, y);
        if (i >= 0) {
            *d = -1;
            return i;
        }
    }
    return -1;
}

static UINT
track(void)
{
    UINT id = 0;
    int minDepth = mt.bar ? 0 : 1;
    for (;;) {
        struct cwev e;
        cw_wait_input(&e);
        if (!W(mt.owner->hwnd))
            return 0;
        if (e.type == EV_ALTTAP)
            return 0;
        if (e.type == EV_KEY) {
            int vk = cw_vkey(e.key), ch = e.key & 0xff;
            int d = mt.depth - 1;
            if (vk == VK_ESCAPE) {
                if (mt.depth > minDepth && (mt.bar || mt.depth > 1)) {
                    mt.depth--;
                    cw_dirty = 1;
                    menuSelect();
                    continue;
                }
                return 0;
            }
            if (vk == VK_F10)
                return 0;
            if (vk == VK_LEFT || vk == VK_RIGHT) {
                int dir = vk == VK_LEFT ? -1 : 1;
                if (dir < 0 && mt.depth > 1) {
                    mt.depth--;
                    cw_dirty = 1;
                    continue;
                }
                if (dir > 0 && d >= 0 && mt.pop[d].sel >= 0 &&
                    (mt.pop[d].m->items[mt.pop[d].sel].flags & MF_POPUP)) {
                    openSub(1);
                    continue;
                }
                if (mt.bar && mt.bar->n) {
                    int open = mt.depth > 0;
                    int i = (mt.barSel + dir + mt.bar->n) % mt.bar->n;
                    if (open && (mt.bar->items[i].flags & MF_POPUP))
                        openBar(i, 1);
                    else {
                        mt.depth = 0;
                        mt.barSel = i;
                        cw_dirty = 1;
                        menuSelect();
                    }
                }
                continue;
            }
            if (vk == VK_UP || vk == VK_DOWN) {
                if (d < 0) {
                    if (mt.bar && mt.barSel >= 0 && (mt.bar->items[mt.barSel].flags & MF_POPUP))
                        openBar(mt.barSel, 1);
                } else {
                    moveSel(d, vk == VK_UP ? -1 : 1);
                }
                continue;
            }
            if (vk == VK_HOME || vk == VK_END) {
                if (d >= 0) {
                    mt.pop[d].sel = vk == VK_HOME ? -1 : 0;
                    moveSel(d, vk == VK_HOME ? 1 : -1);
                }
                continue;
            }
            if (vk == VK_RETURN) {
                if (d < 0) {
                    if (mt.bar && mt.barSel >= 0 && choose(mt.bar, mt.barSel, &id, 1))
                        return id;
                } else if (mt.pop[d].sel >= 0 && choose(mt.pop[d].m, mt.pop[d].sel, &id, 1)) {
                    return id;
                }
                continue;
            }
            /* A mnemonic, with or without Alt. */
            int c = ch ? cw_upper(ch) : (vk < 0x80 ? vk : 0);
            if (c > ' ') {
                struct menu *m = d >= 0 ? mt.pop[d].m : mt.bar;
                int i = findKey(m, c);
                if (i < 0 && d >= 0 && !ch && mt.bar) {
                    /* Alt+letter in a pull-down: another menu of the bar. */
                    m = mt.bar;
                    i = findKey(m, c);
                }
                if (i >= 0) {
                    if (m != mt.bar)
                        mt.pop[d].sel = i;
                    if (choose(m, i, &id, 1))
                        return id;
                } else {
                    MessageBeep(0);
                }
            }
            continue;
        }
        if (e.type == EV_DOWN || e.type == EV_UP || e.type == EV_MOVE) {
            int d, i = menuHit(e.x, e.y, &d);
            int pressed = e.type == EV_DOWN || (e.type == EV_MOVE && e.buttons);
            if (d >= 0) {
                if (i < 0)
                    continue;
                if (mt.pop[d].sel != i || mt.depth != d + 1) {
                    mt.pop[d].sel = i;
                    mt.depth = d + 1;
                    cw_dirty = 1;
                    menuSelect();
                    if (pressed && (mt.pop[d].m->items[i].flags & MF_POPUP))
                        openSub(0);
                }
                if (e.type == EV_UP && choose(mt.pop[d].m, i, &id, 0))
                    return id;
            } else if (d == -1) {
                if (pressed && (i != mt.barSel || mt.depth == 0))
                    openBar(i, 0);
                if (e.type == EV_UP && !(mt.bar->items[i].flags & MF_POPUP) && choose(mt.bar, i, &id, 0))
                    return id;
            } else if (e.type == EV_DOWN) {
                return 0;
            }
        }
    }
}

static UINT
run(void)
{
    mt.active = 1;
    cw_dirty = 1;
    UINT id = track();
    struct wnd *owner = mt.owner;
    mt.active = 0;
    mt.depth = 0;
    mt.barSel = -1;
    cw_dirty = 1;
    if (W(owner->hwnd)) {
        cw_send(owner, WM_MENUSELECT, 0, MAKELONG(0xffff, 0));
        if (id)
            cw_post(owner, WM_COMMAND, id, 0);
    }
    return id;
}

void
cw_track_bar(struct wnd *w, int item, BOOL open, BOOL mouse)
{
    struct menu *m = M(w->menu);
    if (!m || !m->n || mt.active)
        return;
    memset(&mt, 0, sizeof mt);
    mt.owner = w;
    mt.barwnd = w;
    mt.bar = m;
    mt.active = 1;
    if (open)
        openBar(item, !mouse);
    else {
        mt.barSel = item;
        menuSelect();
    }
    run();
}

BOOL
TrackPopupMenu(HMENU h, UINT flags, int x, int y, int reserved, HWND owner, const RECT *rc)
{
    (void)flags;
    (void)reserved;
    (void)rc;
    struct menu *m = M(h);
    struct wnd *w = W(owner);
    if (!m || !w || mt.active)
        return FALSE;
    memset(&mt, 0, sizeof mt);
    mt.owner = w;
    mt.barSel = -1;
    mt.active = 1;
    openPopup(0, m, x, y, 0, 1);
    run();
    return TRUE;
}
