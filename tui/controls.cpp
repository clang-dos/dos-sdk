/* Dialogs and their controls, and scrolling text.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "tui-local.h"
#include <stdlib.h>
#include <string.h>

namespace tui {

static char
upper(char c)
{
    return c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
}

/* Is e the hot key of a title: Alt+letter, or the letter alone when the
 * focused view did not take it? */
static bool
isHotKey(const Event &e, const char *title)
{
    char h = hotChar(title);
    if (!h)
        return false;
    if ((e.key & 0xff) == 0)
        return altKey(h) == e.key;
    return upper(e.ch()) == h;
}

/* ---- Dialogs. ---- */

Dialog::Dialog(const Rect &b, const char *t) : Window(b, t)
{
    dialogColors = true;
    flags = wfMove | wfClose;
}

void
Dialog::handleEvent(Event &e)
{
    Window::handleEvent(e);
    bool modal = app && app->modal == this;
    if (e.what == evKeyDown) {
        switch (e.key) {
        case kbEsc:
            clearEvent(e);
            app->putCommand(cmCancel);
            break;
        case kbEnter: {
            clearEvent(e);
            Event b;
            b.what = evBroadcast;
            b.command = cmDefault;
            Group::handleEvent(b);
            break;
        }
        case kbTab:
        case kbShiftTab:
            clearEvent(e);
            selectNext(e.key == kbShiftTab);
            break;
        }
    } else if (e.what == evCommand && modal) {
        switch (e.command) {
        case cmOK:
        case cmYes:
            if (current && !current->valid(e.command))
                break;
            /* fall through */
        case cmCancel:
        case cmNo:
            endModal(e.command);
            clearEvent(e);
            break;
        }
    }
}

void
Dialog::close()
{
    if (app && app->modal == this)
        endModal(cmCancel);
    else
        Window::close();
}

/* ---- Static text and labels. ---- */

StaticText::StaticText(const Rect &b, const char *t) : View(b), text(t)
{
}

void
StaticText::draw(const DrawContext &dc)
{
    unsigned char a = color::DlgText;
    dc.fill(0, 0, bounds.w, bounds.h, ' ', a);
    const char *p = text;
    int y = 0;
    while (*p && y < bounds.h) {
        bool center = *p == '\003';
        if (center)
            p++;
        /* The longest run of words that fits. */
        int n = 0, cut = -1;
        while (p[n] && p[n] != '\n' && n < bounds.w) {
            if (p[n] == ' ')
                cut = n;
            n++;
        }
        if (p[n] && p[n] != '\n' && p[n] != ' ' && cut > 0)
            n = cut;
        int x = center ? (bounds.w - n) / 2 : 0;
        for (int i = 0; i < n; i++)
            dc.put(x + i, y, (unsigned char)p[i], a);
        p += n;
        if (*p == '\n' || *p == ' ')
            p++;
        y++;
    }
}

Label::Label(const Rect &b, const char *t, View *l) : View(b), text(t), link(l)
{
    options |= ofPostProcess;
}

void
Label::draw(const DrawContext &dc)
{
    bool f = link && link->focused();
    dc.fill(0, 0, bounds.w, bounds.h, ' ', color::Label);
    dc.text(1, 0, text, f ? color::LabelFocus : color::Label, color::LabelHot);
}

void
Label::handleEvent(Event &e)
{
    if (!link)
        return;
    if ((e.what == evMouseDown && mouseIn(e)) ||
        (e.what == evKeyDown && (e.key & 0xff) == 0 && isHotKey(e, text))) {
        link->select();
        clearEvent(e);
    }
}

/* ---- Buttons. ---- */

Button::Button(const Rect &b, const char *t, unsigned c, bool d)
    : View(b), title(t), command(c), isDefault(d), down(false)
{
    options |= ofSelectable | ofPostProcess;
}

void
Button::draw(const DrawContext &dc)
{
    int w = bounds.w - 1;
    dc.fill(0, 0, bounds.w, bounds.h, ' ', color::ButtonShadow);
    bool f = focused();
    unsigned char a = f ? color::ButtonFocus : isDefault ? color::ButtonDefault : color::Button;
    int x = down ? 1 : 0;
    dc.fill(x, 0, w, 1, ' ', a);
    int tw = cstrlen(title);
    dc.text(x + (w - tw) / 2, 0, title, a, color::ButtonHot);
    if (f) {
        dc.put(x, 0, 0x10, a);
        dc.put(x + w - 1, 0, 0x11, a);
    }
    if (!down) {
        dc.put(w, 0, 0xdc, color::ButtonShadow);
        dc.fill(1, 1, w, 1, 0xdf, color::ButtonShadow);
    }
}

void
Button::press()
{
    app->putCommand(command, this);
}

void
Button::handleEvent(Event &e)
{
    if (e.what == evMouseDown && mouseIn(e)) {
        clearEvent(e);
        /* Pressed while the button is held over it. */
        down = true;
        for (;;) {
            app->redraw();
            Event ev;
            app->getEvent(ev);
            if (ev.what == evMouseMove)
                down = mouseIn(ev);
            if (ev.what == evMouseUp) {
                bool in = mouseIn(ev);
                down = false;
                if (in)
                    press();
                break;
            }
        }
    } else if (e.what == evKeyDown) {
        if ((focused() && (e.key == kbSpace || e.key == kbEnter)) || isHotKey(e, title)) {
            clearEvent(e);
            press();
        }
    } else if (e.what == evBroadcast && e.command == cmDefault && isDefault) {
        clearEvent(e);
        press();
    }
}

/* ---- Input lines. ---- */

InputLine::InputLine(const Rect &b, int max)
    : View(b), maxLen(max), pos(0), first(0), selAll(false)
{
    options |= ofSelectable;
    data = (char *)calloc(1, (size_t)max + 1);
}

InputLine::~InputLine()
{
    free(data);
}

void
InputLine::setText(const char *s)
{
    strncpy(data, s, (size_t)maxLen);
    data[maxLen] = 0;
    pos = (int)strlen(data);
    first = 0;
}

void
InputLine::focusChanged(bool f)
{
    selAll = f && data[0];
    if (f)
        pos = (int)strlen(data);
}

void
InputLine::draw(const DrawContext &dc)
{
    int w = bounds.w - 2, len = (int)strlen(data);
    if (pos < first)
        first = pos;
    if (pos >= first + w)
        first = pos - w + 1;
    dc.fill(0, 0, bounds.w, 1, ' ', color::Input);
    for (int i = 0; i < w && first + i < len; i++)
        dc.put(1 + i, 0, (unsigned char)data[first + i], selAll ? color::InputSel : color::Input);
    if (first > 0)
        dc.put(0, 0, 0x11, color::InputArrow);
    if (len - first > w)
        dc.put(bounds.w - 1, 0, 0x10, color::InputArrow);
    if (focused()) {
        scr::curX = absX() + 1 + pos - first;
        scr::curY = absY();
    }
}

void
InputLine::handleEvent(Event &e)
{
    int len = (int)strlen(data);
    if (e.what == evMouseDown && mouseIn(e)) {
        pos = imin(len, imax(0, e.x - absX() - 1 + first));
        selAll = false;
        clearEvent(e);
        return;
    }
    if (e.what != evKeyDown || !focused())
        return;
    unsigned k = e.key;
    char c = e.ch();
    if (k == kbLeft) {
        if (pos > 0)
            pos--;
    } else if (k == kbRight) {
        if (pos < len)
            pos++;
    } else if (k == kbHome) {
        pos = 0;
    } else if (k == kbEnd) {
        pos = len;
    } else if (k == kbBack) {
        if (selAll) {
            data[0] = 0;
            pos = 0;
        } else if (pos > 0) {
            memmove(data + pos - 1, data + pos, (size_t)(len - pos + 1));
            pos--;
        }
    } else if (k == kbDel) {
        if (selAll) {
            data[0] = 0;
            pos = 0;
        } else if (pos < len) {
            memmove(data + pos, data + pos + 1, (size_t)(len - pos));
        }
    } else if (c == 0x19) { /* Ctrl+Y */
        data[0] = 0;
        pos = 0;
    } else if ((unsigned char)c >= 32) {
        if (selAll) {
            data[0] = 0;
            pos = len = 0;
        }
        if (len < maxLen) {
            memmove(data + pos + 1, data + pos, (size_t)(len - pos + 1));
            data[pos++] = c;
        }
    } else {
        return;
    }
    selAll = false;
    clearEvent(e);
}

/* ---- Check boxes and radio buttons. ---- */

Cluster::Cluster(const Rect &b, const char *const *it) : View(b), items(it), count(0), value(0), sel(0)
{
    options |= ofSelectable | ofPostProcess;
    while (items[count])
        count++;
}

void
Cluster::draw(const DrawContext &dc)
{
    bool f = focused();
    dc.fill(0, 0, bounds.w, bounds.h, ' ', color::Cluster);
    const char *ic = icon();
    for (int i = 0; i < count && i < bounds.h; i++) {
        unsigned char a = f && i == sel ? color::ClusterFocus : color::Cluster;
        dc.text(1, i, ic, a);
        if (marked(i))
            dc.put(2, i, (unsigned char)mark(), a);
        dc.text(5, i, items[i], a, color::ClusterHot);
    }
    if (f) {
        scr::curX = absX() + 2;
        scr::curY = absY() + sel;
    }
}

void
Cluster::focusChanged(bool f)
{
    /* Radio buttons start at the chosen one. */
    if (f && mark() != 'X' && (int)value < count)
        sel = (int)value;
}

void
Cluster::handleEvent(Event &e)
{
    bool radio = mark() != 'X';
    if (e.what == evMouseDown && mouseIn(e)) {
        int i = e.y - absY();
        if (i >= 0 && i < count) {
            sel = i;
            press(i);
        }
        clearEvent(e);
        return;
    }
    if (e.what != evKeyDown)
        return;
    if (focused()) {
        if (e.key == kbUp || e.key == kbDown) {
            sel = (sel + (e.key == kbUp ? count - 1 : 1)) % count;
            if (radio)
                press(sel);
            clearEvent(e);
            return;
        }
        if (e.key == kbSpace) {
            press(sel);
            clearEvent(e);
            return;
        }
    }
    for (int i = 0; i < count; i++)
        if (isHotKey(e, items[i]) && ((e.key & 0xff) == 0 || focused())) {
            select();
            sel = i;
            press(i);
            clearEvent(e);
            return;
        }
}

/* ---- List boxes. ---- */

static void
scrollBar(const DrawContext &dc, int x, int h, int pos, int total)
{
    dc.put(x, 0, 0x1e, color::ScrollBar);
    dc.put(x, h - 1, 0x1f, color::ScrollBar);
    dc.fill(x, 1, 1, h - 2, 0xb1, color::ScrollBar);
    if (h > 2 && total > 1)
        dc.put(x, 1 + (int)((long)pos * (h - 3) / (total - 1)), 0xfe, color::ScrollBar);
}

ListBox::ListBox(const Rect &b, const char *const *it, int n) : View(b), items(it), count(n), sel(0), top(0)
{
    options |= ofSelectable;
}

void
ListBox::setItems(const char *const *it, int n)
{
    items = it;
    count = n;
    sel = top = 0;
}

void
ListBox::focusItem(int i)
{
    if (count == 0)
        return;
    sel = imax(0, imin(i, count - 1));
    if (sel < top)
        top = sel;
    if (sel >= top + bounds.h)
        top = sel - bounds.h + 1;
}

void
ListBox::draw(const DrawContext &dc)
{
    bool f = focused();
    int w = bounds.w - 1;
    dc.fill(0, 0, bounds.w, bounds.h, ' ', color::List);
    for (int r = 0; r < bounds.h && top + r < count; r++) {
        int i = top + r;
        unsigned char a = i == sel ? (f ? color::ListFocus : color::ListSel) : color::List;
        dc.fill(0, r, w, 1, ' ', a);
        const char *s = items[i];
        for (int k = 0; s[k] && k < w - 1; k++)
            dc.put(1 + k, r, (unsigned char)s[k], a);
    }
    scrollBar(dc, w, bounds.h, sel, count);
}

void
ListBox::handleEvent(Event &e)
{
    int h = bounds.h;
    if (e.what == evMouseDown && mouseIn(e)) {
        int x = e.x - absX(), y = e.y - absY();
        if (x == bounds.w - 1) {
            if (y == 0)
                focusItem(sel - 1);
            else if (y == h - 1)
                focusItem(sel + 1);
            else
                focusItem(y < h / 2 ? sel - h : sel + h);
        } else if (top + y < count) {
            focusItem(top + y);
            if (e.dbl)
                app->putCommand(cmListSelect, this);
        }
        clearEvent(e);
        return;
    }
    if (e.what != evKeyDown || !focused())
        return;
    switch (e.key) {
    case kbUp:
        focusItem(sel - 1);
        break;
    case kbDown:
        focusItem(sel + 1);
        break;
    case kbPgUp:
        focusItem(sel - h + 1);
        break;
    case kbPgDn:
        focusItem(sel + h - 1);
        break;
    case kbHome:
        focusItem(0);
        break;
    case kbEnd:
        focusItem(count - 1);
        break;
    default: {
        char c = upper(e.ch());
        if (c <= ' ' || (e.key & 0xff) == 0)
            return;
        /* The next item that starts with the letter. */
        for (int k = 1; k <= count; k++) {
            int i = (sel + k) % count;
            if (upper(items[i][0]) == c) {
                focusItem(i);
                break;
            }
        }
        break;
    }
    }
    clearEvent(e);
}

/* ---- Text views. ---- */

TextView::TextView(const Rect &b)
    : View(b), textAttr(color::WinText), lines(0), nlines(0), cap(0), top(0), left(0)
{
    options |= ofSelectable;
    growMode = gfGrowSize;
}

TextView::~TextView()
{
    clear();
}

void
TextView::clear()
{
    for (int i = 0; i < nlines; i++)
        free(lines[i]);
    free(lines);
    lines = 0;
    nlines = cap = top = left = 0;
}

void
TextView::addLine(const char *s)
{
    if (nlines == cap) {
        int ncap = cap ? cap * 2 : 16;
        char **n = (char **)realloc(lines, sizeof(char *) * (size_t)ncap);
        if (!n)
            return;
        lines = n;
        cap = ncap;
    }
    lines[nlines++] = dupstr(s);
}

void
TextView::addText(const char *s)
{
    char line[256];
    while (*s) {
        int n = 0;
        while (s[n] && s[n] != '\n' && n < 255)
            n++;
        memcpy(line, s, (size_t)n);
        line[n] = 0;
        /* Tabs to spaces, and no CR. */
        char out[256];
        int o = 0;
        for (int i = 0; i < n && o < 255; i++) {
            if (line[i] == '\r')
                continue;
            if (line[i] == '\t') {
                do
                    out[o++] = ' ';
                while ((o & 7) && o < 255);
            } else {
                out[o++] = line[i];
            }
        }
        out[o] = 0;
        addLine(out);
        s += n;
        if (*s == '\n')
            s++;
    }
}

int
TextView::width() const
{
    return bounds.w - 1;
}

void
TextView::draw(const DrawContext &dc)
{
    int w = width();
    dc.fill(0, 0, bounds.w, bounds.h, ' ', textAttr);
    for (int r = 0; r < bounds.h && top + r < nlines; r++) {
        const char *s = lines[top + r];
        int len = (int)strlen(s);
        for (int k = 0; k < w && left + k < len; k++)
            dc.put(k, r, (unsigned char)s[left + k], textAttr);
    }
    scrollBar(dc, w, bounds.h, top, imax(1, nlines - bounds.h + 1));
}

void
TextView::handleEvent(Event &e)
{
    int h = bounds.h, maxTop = imax(0, nlines - h);
    int t = top, l = left;
    if (e.what == evMouseDown && mouseIn(e)) {
        int x = e.x - absX(), y = e.y - absY();
        if (x == bounds.w - 1) {
            if (y == 0)
                t--;
            else if (y == h - 1)
                t++;
            else
                t += y < h / 2 ? -h : h;
        }
    } else if (e.what == evKeyDown && focused()) {
        switch (e.key) {
        case kbUp: t--; break;
        case kbDown: t++; break;
        case kbPgUp: t -= h - 1; break;
        case kbPgDn: t += h - 1; break;
        case kbHome: l = 0; break;
        case kbEnd: t = maxTop; break;
        case kbLeft: l -= 4; break;
        case kbRight: l += 4; break;
        default: return;
        }
    } else {
        return;
    }
    top = imax(0, imin(t, maxTop));
    left = imax(0, imin(l, 200));
    clearEvent(e);
}

} // namespace tui
