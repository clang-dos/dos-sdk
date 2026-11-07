/* The menu bar, its pull-down menus, and the status line.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "tui-local.h"
#include <string.h>

namespace tui {

/* ---- Building menus. ---- */

Menu::Menu() : name(0), items(0), next(0)
{
}

static Menu &
append(Menu &m, const char *name, unsigned command, unsigned key, const char *keyName, Menu *sub)
{
    MenuItem *it = new MenuItem{ name, command, key, keyName, sub, false, 0 };
    MenuItem **p = &m.items;
    while (*p)
        p = &(*p)->next;
    *p = it;
    return m;
}

Menu &
Menu::item(const char *n, unsigned command, unsigned key, const char *keyName)
{
    return append(*this, n, command, key, keyName, 0);
}

Menu &
Menu::separator()
{
    return append(*this, 0, 0, 0, 0, 0);
}

Menu &
Menu::submenu(const char *n, Menu *sub)
{
    return append(*this, n, 0, 0, 0, sub);
}

static void
freeMenu(Menu *m)
{
    while (m) {
        MenuItem *it = m->items;
        while (it) {
            MenuItem *n = it->next;
            if (it->submenu)
                freeMenu(it->submenu);
            delete it;
            it = n;
        }
        Menu *n = m->next;
        delete m;
        m = n;
    }
}

/* Find the item with a shortcut key. */
static MenuItem *
findKey(Menu *m, unsigned key)
{
    for (; m; m = m->next)
        for (MenuItem *it = m->items; it; it = it->next) {
            if (it->name && it->key == key && !it->disabled && it->command)
                return it;
            if (it->submenu)
                if (MenuItem *s = findKey(it->submenu, key))
                    return s;
        }
    return 0;
}

static void
enableAll(Menu *m, unsigned command, bool on)
{
    for (; m; m = m->next)
        for (MenuItem *it = m->items; it; it = it->next) {
            if (it->command == command)
                it->disabled = !on;
            if (it->submenu)
                enableAll(it->submenu, command, on);
        }
}

/* ---- A pulled-down menu. ---- */

class MenuBox : public View {
public:
    MenuBox(Menu *m, int x, int y);
    Menu *menu;
    int sel;
    int count;
    MenuItem *at(int i) const;
    int itemAt(const Event &e) const;
    void draw(const DrawContext &dc) override;
    bool usable(int i) const;
    void step(int d);
};

static Rect
boxRect(Menu *m, int x, int y)
{
    int w = 10, n = 0;
    for (MenuItem *it = m->items; it; it = it->next, n++) {
        if (!it->name)
            continue;
        int iw = cstrlen(it->name) + (it->keyName ? (int)strlen(it->keyName) + 3 : 0) +
                 (it->submenu ? 3 : 0) + 4;
        w = imax(w, iw);
    }
    if (x + w > scr::cols)
        x = imax(0, scr::cols - w);
    return Rect(x, y, w, n + 2);
}

MenuBox::MenuBox(Menu *m, int x, int y) : View(boxRect(m, x, y)), menu(m), sel(0), count(0)
{
    options |= ofShadow;
    for (MenuItem *it = m->items; it; it = it->next)
        count++;
    if (!usable(0))
        step(1);
}

MenuItem *
MenuBox::at(int i) const
{
    MenuItem *it = menu->items;
    while (it && i-- > 0)
        it = it->next;
    return it;
}

bool
MenuBox::usable(int i) const
{
    MenuItem *it = at(i);
    return it && it->name && !it->disabled;
}

void
MenuBox::step(int d)
{
    for (int k = 0; k < count; k++) {
        sel = (sel + d + count) % count;
        if (usable(sel))
            return;
    }
}

int
MenuBox::itemAt(const Event &e) const
{
    int x = e.x - absX(), y = e.y - absY();
    if (x < 1 || x >= bounds.w - 1 || y < 1 || y > count)
        return -1;
    return y - 1;
}

void
MenuBox::draw(const DrawContext &dc)
{
    int w = bounds.w;
    dc.fill(0, 0, w, bounds.h, ' ', color::MenuText);
    dc.box(0, 0, w, bounds.h, color::MenuText, false);
    int i = 0;
    for (MenuItem *it = menu->items; it; it = it->next, i++) {
        int y = i + 1;
        if (!it->name) {
            dc.put(0, y, 0xc3, color::MenuText);
            dc.fill(1, y, w - 2, 1, 0xc4, color::MenuText);
            dc.put(w - 1, y, 0xb4, color::MenuText);
            continue;
        }
        bool s = i == sel;
        unsigned char a = it->disabled ? color::MenuDisabled : s ? color::MenuSel : color::MenuText;
        unsigned char h = it->disabled ? a : s ? color::MenuSelHot : color::MenuHot;
        dc.fill(1, y, w - 2, 1, ' ', a);
        dc.text(2, y, it->name, a, h);
        if (it->keyName)
            dc.text(w - 2 - (int)strlen(it->keyName), y, it->keyName, a);
        if (it->submenu)
            dc.put(w - 3, y, 0x10, a);
    }
}

/* What a pulled-down menu's loop ended with. */
enum { mrCancel = 0, mrLeft = -1, mrRight = -2, mrOther = -3, mrOutside = -4 };

/* Run a pulled-down menu at (x, y). Returns a command, or an mr code;
 * for mrOther, *other is the menu bar item the mouse or an Alt key went
 * to. */
static int
runBox(MenuBar *bar, Menu *m, int x, int y, int *other)
{
    MenuBox *box = new MenuBox(m, x, y);
    app->insert(box);
    int result = mrCancel;
    for (;;) {
        app->redraw();
        Event e;
        app->getEvent(e);
        int choose = -1;
        if (e.what == evKeyDown) {
            unsigned k = e.key;
            if (k == kbEsc) {
                result = mrCancel;
                break;
            } else if (k == kbUp) {
                box->step(-1);
            } else if (k == kbDown) {
                box->step(1);
            } else if (k == kbHome) {
                box->sel = box->count - 1;
                box->step(1);
            } else if (k == kbEnd) {
                box->sel = 0;
                box->step(-1);
            } else if (k == kbLeft) {
                result = mrLeft;
                break;
            } else if (k == kbRight) {
                MenuItem *it = box->at(box->sel);
                if (it && it->submenu)
                    choose = box->sel;
                else {
                    result = mrRight;
                    break;
                }
            } else if (k == kbEnter) {
                choose = box->sel;
            } else if ((k & 0xff) == 0) {
                /* Alt+letter of another menu on the bar. */
                int i = 0;
                for (Menu *mm = bar->menus; mm; mm = mm->next, i++)
                    if (mm->name && altKey(hotChar(mm->name)) == k) {
                        *other = i;
                        result = mrOther;
                        break;
                    }
                if (result == mrOther)
                    break;
            } else {
                char c = e.ch();
                if (c >= 'a' && c <= 'z')
                    c = (char)(c - 32);
                for (int i = 0; i < box->count; i++)
                    if (box->usable(i) && hotChar(box->at(i)->name) == c) {
                        box->sel = i;
                        choose = i;
                        break;
                    }
            }
        } else if (e.what & evMouse) {
            int i = box->mouseIn(e) ? box->itemAt(e) : -1;
            if (i >= 0 && box->usable(i) && (e.buttons || e.what == evMouseUp))
                box->sel = i;
            if (e.what == evMouseUp && i >= 0 && box->usable(i)) {
                choose = i;
            } else if (e.y == bar->absY() && e.buttons && !box->mouseIn(e)) {
                /* On the menu bar: another menu, or a click on this one's
                 * title, which closes it. */
                int k = bar->menuAt(e.x - bar->absX());
                if (k >= 0 && k != bar->open) {
                    *other = k;
                    result = mrOther;
                    break;
                }
                if (e.what == evMouseDown) {
                    result = k == bar->open ? mrCancel : mrOutside;
                    break;
                }
            } else if (e.what == evMouseDown && !box->mouseIn(e)) {
                result = mrOutside;
                break;
            }
        }
        if (choose >= 0) {
            MenuItem *it = box->at(choose);
            if (it->submenu) {
                int r = runBox(bar, it->submenu, box->bounds.x + box->bounds.w - 2,
                               box->bounds.y + choose + 1, other);
                if (r > 0 || r == mrRight || r == mrOther || r == mrOutside) {
                    result = r;
                    break;
                }
                continue;
            }
            result = (int)it->command;
            break;
        }
    }
    app->remove(box);
    delete box;
    return result;
}

/* ---- The menu bar. ---- */

MenuBar::MenuBar(const Rect &b) : View(b), menus(0), open(-1)
{
    options |= ofPreProcess;
}

MenuBar::~MenuBar()
{
    freeMenu(menus);
}

Menu &
MenuBar::add(const char *name)
{
    Menu *m = new Menu;
    m->name = name;
    Menu **p = &menus;
    while (*p)
        p = &(*p)->next;
    *p = m;
    return *m;
}

void
MenuBar::enable(unsigned command, bool on)
{
    enableAll(menus, command, on);
}

int
MenuBar::menuX(int index) const
{
    int x = 1, i = 0;
    for (Menu *m = menus; m && i < index; m = m->next, i++)
        x += cstrlen(m->name) + 2;
    return x;
}

int
MenuBar::menuAt(int x) const
{
    int i = 0;
    for (Menu *m = menus; m; m = m->next, i++) {
        int mx = menuX(i);
        if (x >= mx && x < mx + cstrlen(m->name) + 2)
            return i;
    }
    return -1;
}

void
MenuBar::draw(const DrawContext &dc)
{
    dc.fill(0, 0, bounds.w, 1, ' ', color::MenuText);
    int i = 0;
    for (Menu *m = menus; m; m = m->next, i++) {
        bool s = i == open;
        int x = menuX(i);
        unsigned char a = s ? color::MenuSel : color::MenuText;
        dc.put(x, 0, ' ', a);
        int n = dc.text(x + 1, 0, m->name, a, s ? color::MenuSelHot : color::MenuHot);
        dc.put(x + 1 + n, 0, ' ', a);
    }
}

unsigned
MenuBar::execute(int index, Event *)
{
    int count = 0;
    for (Menu *m = menus; m; m = m->next)
        count++;
    if (!count)
        return 0;
    open = index;
    unsigned result = 0;
    for (;;) {
        Menu *m = menus;
        for (int i = 0; i < open; i++)
            m = m->next;
        int other = -1;
        int r = runBox(this, m, menuX(open) - 1, absY() + 1, &other);
        if (r > 0) {
            result = (unsigned)r;
            break;
        }
        if (r == mrLeft) {
            open = (open + count - 1) % count;
        } else if (r == mrRight) {
            open = (open + 1) % count;
        } else if (r == mrOther) {
            open = other;
        } else {
            break;
        }
    }
    open = -1;
    return result;
}

void
MenuBar::handleEvent(Event &e)
{
    int start = -1;
    if (e.what == evKeyDown) {
        if (e.key == kbF10) {
            start = 0;
        } else if ((e.key & 0xff) == 0 && e.key) {
            int i = 0;
            for (Menu *m = menus; m; m = m->next, i++)
                if (altKey(hotChar(m->name)) == e.key)
                    start = i;
        }
        if (start < 0) {
            if (MenuItem *it = findKey(menus, e.key)) {
                clearEvent(e);
                app->putCommand(it->command);
            }
            return;
        }
    } else if (e.what == evCommand && e.command == cmMenu) {
        start = 0;
    } else if (e.what == evMouseDown && mouseIn(e)) {
        start = menuAt(e.x - absX());
        if (start < 0) {
            clearEvent(e);
            return;
        }
    } else {
        return;
    }
    clearEvent(e);
    unsigned c = execute(start, &e);
    if (c)
        app->putCommand(c);
}

/* ---- The status line. ---- */

StatusLine::StatusLine(const Rect &b) : View(b), items(0), hint(0)
{
    options |= ofPreProcess;
}

StatusLine::~StatusLine()
{
    while (items) {
        StatusItem *n = items->next;
        delete items;
        items = n;
    }
}

StatusLine &
StatusLine::add(const char *text, unsigned key, unsigned command)
{
    StatusItem *it = new StatusItem{ text, key, command, 0 };
    StatusItem **p = &items;
    while (*p)
        p = &(*p)->next;
    *p = it;
    return *this;
}

void
StatusLine::draw(const DrawContext &dc)
{
    dc.fill(0, 0, bounds.w, 1, ' ', color::MenuText);
    int x = 1;
    for (StatusItem *it = items; it; it = it->next) {
        if (!it->text)
            continue;
        x += dc.text(x, 0, it->text, color::MenuText, color::MenuHot) + 2;
    }
    if (hint) {
        dc.put(x - 1, 0, 0xb3, color::MenuText);
        dc.text(x + 1, 0, hint, color::MenuText);
    }
}

void
StatusLine::handleEvent(Event &e)
{
    if (e.what == evKeyDown) {
        for (StatusItem *it = items; it; it = it->next)
            if (it->key && it->key == e.key) {
                clearEvent(e);
                app->putCommand(it->command);
                return;
            }
    } else if (e.what == evMouseDown && mouseIn(e)) {
        int x = 1, mx = e.x - absX();
        for (StatusItem *it = items; it; it = it->next) {
            if (!it->text)
                continue;
            int w = cstrlen(it->text);
            if (mx >= x && mx < x + w) {
                app->putCommand(it->command);
                break;
            }
            x += w + 2;
        }
        clearEvent(e);
    }
}

} // namespace tui
