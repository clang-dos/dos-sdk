/* Views and groups of views.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "tui-local.h"

namespace tui {

View::View(const Rect &b)
    : bounds(b), owner(0), next(0), options(0), visible(true), helpCtx(0), growMode(0)
{
}

void
View::setBounds(const Rect &r)
{
    bounds = r;
}

View::~View()
{
    if (owner)
        owner->remove(this);
}

void
View::draw(const DrawContext &dc)
{
    dc.fill(0, 0, bounds.w, bounds.h, ' ', 0x07);
}

void
View::handleEvent(Event &)
{
}

void
View::focusChanged(bool)
{
}

bool
View::valid(unsigned)
{
    return true;
}

void
View::select()
{
    if (!owner)
        return;
    owner->setCurrent(this);
    if (options & ofTopSelect)
        owner->toFront(this);
    owner->select();
}

bool
View::focused() const
{
    if (!owner)
        return true;
    return owner->current == this && owner->focused();
}

int
View::absX() const
{
    return owner ? owner->absX() + owner->client().x + bounds.x : bounds.x;
}

int
View::absY() const
{
    return owner ? owner->absY() + owner->client().y + bounds.y : bounds.y;
}

bool
View::mouseIn(const Event &e) const
{
    return Rect(absX(), absY(), bounds.w, bounds.h).contains(e.x, e.y);
}

/* ---- Groups. ---- */

Group::Group(const Rect &b) : View(b), first(0), current(0), endState(0)
{
}

Group::~Group()
{
    while (first) {
        View *v = first;
        remove(v);
        delete v;
    }
}

Rect
Group::client() const
{
    return Rect(0, 0, bounds.w, bounds.h);
}

void
Group::setBounds(const Rect &r)
{
    int dw = r.w - bounds.w, dh = r.h - bounds.h;
    bounds = r;
    if (!dw && !dh)
        return;
    for (View *v = first; v; v = v->next) {
        Rect b = v->bounds;
        unsigned g = v->growMode;
        int x1 = b.x + b.w, y1 = b.y + b.h;
        if (g & gfGrowLoX)
            b.x += dw;
        if (g & gfGrowLoY)
            b.y += dh;
        if (g & gfGrowHiX)
            x1 += dw;
        if (g & gfGrowHiY)
            y1 += dh;
        b.w = imax(1, x1 - b.x);
        b.h = imax(1, y1 - b.y);
        v->setBounds(b);
    }
}

void
Group::insert(View *v)
{
    v->owner = this;
    v->next = 0;
    if (v->options & ofCentered) {
        Rect c = client();
        v->bounds.x = (c.w - v->bounds.w) / 2;
        v->bounds.y = (c.h - v->bounds.h) / 2;
    }
    if (!first) {
        first = v;
    } else {
        View *p = first;
        while (p->next)
            p = p->next;
        p->next = v;
    }
    if (v->options & ofSelectable)
        setCurrent(v);
}

void
Group::remove(View *v)
{
    if (v->owner != this)
        return;
    if (first == v) {
        first = v->next;
    } else {
        View *p = first;
        while (p && p->next != v)
            p = p->next;
        if (p)
            p->next = v->next;
    }
    v->owner = 0;
    v->next = 0;
    if (current == v) {
        current = 0;
        v->focusChanged(false);
        /* Focus the topmost view left that can have it. */
        View *top = 0;
        for (View *p = first; p; p = p->next)
            if ((p->options & ofSelectable) && p->visible)
                top = p;
        setCurrent(top);
    }
}

void
Group::toFront(View *v)
{
    if (!v->next)
        return;
    View *saved = current;
    current = 0; /* keep the focus while moving */
    if (first == v) {
        first = v->next;
    } else {
        View *p = first;
        while (p->next != v)
            p = p->next;
        p->next = v->next;
    }
    View *p = first;
    while (p->next)
        p = p->next;
    p->next = v;
    v->next = 0;
    current = saved;
}

void
Group::setCurrent(View *v)
{
    if (current == v)
        return;
    View *old = current;
    current = v;
    if (old)
        old->focusChanged(false);
    if (v)
        v->focusChanged(true);
}

void
Group::focusChanged(bool f)
{
    if (current)
        current->focusChanged(f);
}

void
Group::selectNext(bool backward)
{
    /* The selectable views in order, then the one after (or before) the
     * current one, round. */
    View *list[64];
    int n = 0, at = -1;
    for (View *v = first; v && n < 64; v = v->next) {
        if ((v->options & ofSelectable) && v->visible) {
            if (v == current)
                at = n;
            list[n++] = v;
        }
    }
    if (n == 0)
        return;
    if (current && !current->valid(cmNone))
        return;
    int i = at < 0 ? 0 : (at + (backward ? n - 1 : 1)) % n;
    list[i]->select();
}

View *
Group::childAt(int x, int y) const
{
    View *hit = 0;
    for (View *v = first; v; v = v->next)
        if (v->visible && v->mouseIn(Event{ 0, 0, 0, x, y, 0, false, 0, 0 }))
            hit = v;
    return hit;
}

void
Group::draw(const DrawContext &dc)
{
    DrawContext cc = dc.sub(client());
    for (View *v = first; v; v = v->next) {
        if (!v->visible)
            continue;
        v->draw(cc.sub(v->bounds));
        if (v->options & ofShadow) {
            const Rect &b = v->bounds;
            cc.shade(b.x + b.w, b.y + 1, 2, b.h);
            cc.shade(b.x + 2, b.y + b.h, b.w, 1);
        }
    }
}

void
Group::handleEvent(Event &e)
{
    if (e.what & evMouse) {
        View *v = childAt(e.x, e.y);
        if (!v)
            return;
        if (e.what == evMouseDown && (v->options & ofSelectable) && v != current) {
            if (current && !current->valid(cmNone)) {
                clearEvent(e);
                return;
            }
            v->select();
        }
        v->handleEvent(e);
        return;
    }
    if (e.what == evKeyDown) {
        for (View *v = first; v && e.what; v = v->next)
            if ((v->options & ofPreProcess) && v->visible)
                v->handleEvent(e);
        if (e.what && current)
            current->handleEvent(e);
        for (View *v = first; v && e.what; v = v->next)
            if ((v->options & ofPostProcess) && v->visible && v != current)
                v->handleEvent(e);
        return;
    }
    if (e.what == evCommand) {
        if (current)
            current->handleEvent(e);
        return;
    }
    if (e.what == evBroadcast) {
        for (View *v = first; v && e.what;) {
            View *n = v->next;
            v->handleEvent(e);
            v = n;
        }
    }
}

/* ---- Windows. ---- */

Window::Window(const Rect &b, const char *t, int num)
    : Group(b), title(t), number(num), flags(wfDefault), zoomRect(b),
      dialogColors(false), minW(16), minH(6)
{
    options |= ofSelectable | ofTopSelect | ofShadow;
}

Window::~Window()
{
}

Rect
Window::client() const
{
    return Rect(1, 1, bounds.w - 2, bounds.h - 2);
}

unsigned char
Window::frameColor() const
{
    bool active = focused();
    if (dialogColors)
        return active ? color::DlgFrame : color::DlgFrameIdle;
    return active ? color::WinFrame : color::WinFrameIdle;
}

unsigned char
Window::textColor() const
{
    return dialogColors ? color::DlgText : color::WinText;
}

void
Window::draw(const DrawContext &dc)
{
    bool active = focused();
    unsigned char fc = frameColor();
    int w = bounds.w, h = bounds.h;
    dc.fill(1, 1, w - 2, h - 2, ' ', textColor());
    dc.box(0, 0, w, h, fc, active);
    if (title) {
        int tw = cstrlen(title) + 2, tx = (w - tw) / 2;
        if (tw < w - 10 || tx > 1) {
            dc.put(tx, 0, ' ', fc);
            dc.text(tx + 1, 0, title, fc);
            dc.put(tx + tw - 1, 0, ' ', fc);
        }
    }
    if (active) {
        unsigned char ic = dialogColors ? color::DlgFrame : color::WinIcon;
        if (flags & wfClose) {
            dc.text(2, 0, "[ ]", fc);
            dc.put(3, 0, 0xfe, ic);
        }
        if ((flags & wfZoom) && owner) {
            Rect c = owner->client();
            bool zoomed = bounds.x == 0 && bounds.y == 0 && bounds.w == c.w && bounds.h == c.h;
            dc.text(w - 5, 0, "[ ]", fc);
            dc.put(w - 4, 0, zoomed ? 0x12 : 0x18, ic);
        }
        if (flags & wfGrow) {
            dc.put(w - 2, h - 1, 0xc4, ic);
            dc.put(w - 1, h - 1, 0xd9, ic);
        }
    }
    if (number > 0 && number < 10 && w > 14)
        dc.put(w - 7, 0, (unsigned char)('0' + number), fc);
    Group::draw(dc);
}

void
Window::handleEvent(Event &e)
{
    if (e.what == evMouseDown) {
        int x = e.x - absX(), y = e.y - absY();
        bool active = focused();
        if (y == 0) {
            if (active && (flags & wfClose) && x >= 2 && x <= 4) {
                clearEvent(e);
                close();
                return;
            }
            if (active && (flags & wfZoom) && x >= bounds.w - 5 && x <= bounds.w - 3) {
                clearEvent(e);
                zoom();
                return;
            }
            if (e.dbl && (flags & wfZoom)) {
                clearEvent(e);
                zoom();
                return;
            }
            if (flags & wfMove) {
                dragMove(e);
                return;
            }
        }
        if ((flags & wfGrow) && y == bounds.h - 1 && x >= bounds.w - 2) {
            dragSize(e);
            return;
        }
    }
    Group::handleEvent(e);
    if (e.what == evCommand && focused()) {
        if (e.command == cmClose && (flags & wfClose)) {
            clearEvent(e);
            close();
        } else if (e.command == cmZoom && (flags & wfZoom)) {
            clearEvent(e);
            zoom();
        }
    }
}

void
Window::close()
{
    if (valid(cmClose))
        app->destroy(this);
}

void
Window::zoom()
{
    if (!owner)
        return;
    Rect c = owner->client();
    Rect full(0, 0, c.w, c.h);
    if (bounds.x == 0 && bounds.y == 0 && bounds.w == c.w && bounds.h == c.h) {
        setBounds(zoomRect);
    } else {
        zoomRect = bounds;
        setBounds(full);
    }
}

void
Window::dragMove(Event &e)
{
    int dx = e.x - absX(), dy = e.y - absY();
    int ox = owner->absX() + owner->client().x, oy = owner->absY() + owner->client().y;
    Rect c = owner->client();
    clearEvent(e);
    for (;;) {
        app->redraw();
        Event ev;
        app->getEvent(ev);
        if (ev.what == evMouseMove || ev.what == evMouseUp) {
            int nx = ev.x - ox - dx, ny = ev.y - oy - dy;
            bounds.x = imax(-bounds.w + 4, imin(nx, c.w - 4));
            bounds.y = imax(0, imin(ny, c.h - 1));
        }
        if (ev.what == evMouseUp)
            break;
    }
}

void
Window::dragSize(Event &e)
{
    int ox = absX(), oy = absY();
    Rect c = owner->client();
    int maxW = c.w - bounds.x, maxH = c.h - bounds.y;
    clearEvent(e);
    for (;;) {
        app->redraw();
        Event ev;
        app->getEvent(ev);
        if (ev.what == evMouseMove || ev.what == evMouseUp) {
            setBounds(Rect(bounds.x, bounds.y, imax(minW, imin(ev.x - ox + 1, maxW)),
                           imax(minH, imin(ev.y - oy + 1, maxH))));
        }
        if (ev.what == evMouseUp)
            break;
    }
}

/* ---- The desktop. ---- */

Desktop::Desktop(const Rect &b) : Group(b), pattern(0xb1)
{
}

void
Desktop::draw(const DrawContext &dc)
{
    dc.fill(0, 0, bounds.w, bounds.h, pattern, color::Desktop);
    Group::draw(dc);
}

void
Desktop::handleEvent(Event &e)
{
    Group::handleEvent(e);
    if (e.what == evCommand) {
        switch (e.command) {
        case cmNext:
            /* The bottom window comes to the front. */
            for (View *v = first; v; v = v->next) {
                if ((v->options & ofSelectable) && v->visible && v != current) {
                    v->select();
                    break;
                }
            }
            break;
        case cmPrev:
            if (current && current != first) {
                View *c = current;
                remove(c);
                c->next = first;
                first = c;
                c->owner = this;
                View *top = 0;
                for (View *v = first; v; v = v->next)
                    if (v->options & ofSelectable)
                        top = v;
                setCurrent(top);
            }
            break;
        case cmTile:
            tile();
            break;
        case cmCascade:
            cascade();
            break;
        default:
            return;
        }
        clearEvent(e);
    } else if (e.what == evKeyDown) {
        /* Alt+1 to Alt+9: the window with that number. */
        unsigned scan = e.key >> 8;
        if ((e.key & 0xff) == 0 && scan >= 0x78 && scan <= 0x80) {
            int n = (int)scan - 0x77;
            for (View *v = first; v; v = v->next) {
                Window *w = static_cast<Window *>(v);
                if ((v->options & ofTopSelect) && w->number == n) {
                    v->select();
                    clearEvent(e);
                    break;
                }
            }
        }
    }
}

static int
windows(Group *g, View **list, int max)
{
    int n = 0;
    for (View *v = g->first; v && n < max; v = v->next)
        if ((v->options & ofTopSelect) && v->visible)
            list[n++] = v;
    return n;
}

void
Desktop::tile()
{
    View *list[32];
    int n = windows(this, list, 32);
    if (!n)
        return;
    int cols = 1;
    while (cols * cols < n)
        cols++;
    int rows = (n + cols - 1) / cols;
    for (int i = 0; i < n; i++) {
        int c = i % cols, r = i / cols;
        int cl = r == rows - 1 ? n - (rows - 1) * cols : cols; /* columns in this row */
        if (r == rows - 1)
            c = i - (rows - 1) * cols;
        int x0 = bounds.w * c / cl, x1 = bounds.w * (c + 1) / cl;
        int y0 = bounds.h * r / rows, y1 = bounds.h * (r + 1) / rows;
        list[i]->setBounds(Rect(x0, y0, x1 - x0, y1 - y0));
    }
}

void
Desktop::cascade()
{
    View *list[32];
    int n = windows(this, list, 32);
    if (!n)
        return;
    int step = imin(n - 1, imin(bounds.w - 20, bounds.h - 6));
    for (int i = 0; i < n; i++) {
        int k = imin(i, step);
        list[i]->setBounds(Rect(k, k, bounds.w - step, bounds.h - step));
    }
}

} // namespace tui
