/* The application: the event loop, modal views, and ready-made dialogs.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "tui-local.h"
#include <string.h>

namespace tui {

Application *app;

static Event queue[8];
static int qhead, qcount;
static View *doomed[16];
static int ndoomed;

Application::Application() : Group(Rect(0, 0, 80, 25)), modal(0)
{
    app = this;
    scr::init();
    bounds = Rect(0, 0, scr::cols, scr::rows);
    desktop = new Desktop(Rect(0, 1, scr::cols, scr::rows - 2));
    desktop->options |= ofSelectable;
    menuBar = new MenuBar(Rect(0, 0, scr::cols, 1));
    statusLine = new StatusLine(Rect(0, scr::rows - 1, scr::cols, 1));
    insert(desktop);
    insert(menuBar);
    insert(statusLine);
    modal = this;
}

Application::~Application()
{
    while (first) {
        View *v = first;
        remove(v);
        delete v;
    }
    scr::done();
    app = 0;
}

int
Application::cols() const
{
    return scr::cols;
}

int
Application::rows() const
{
    return scr::rows;
}

bool
Application::hasMouse() const
{
    return scr::mouse;
}

void
Application::putEvent(const Event &e)
{
    if (qcount == 8)
        return;
    queue[(qhead + qcount++) % 8] = e;
}

void
Application::putCommand(unsigned command, void *info)
{
    Event e = Event();
    e.what = evCommand;
    e.command = command;
    e.info = info;
    putEvent(e);
}

void
Application::idle()
{
    scr::release();
}

void
Application::getEvent(Event &e)
{
    if (qcount) {
        e = queue[qhead];
        qhead = (qhead + 1) % 8;
        qcount--;
        return;
    }
    while (!scr::poll(e)) {
        idle();
        if (qcount) {
            getEvent(e);
            return;
        }
    }
    if (e.what == evMouseMove && !e.buttons) {
        /* Show the mouse where it went. */
        scr::flush();
    }
}

void
Application::redraw()
{
    scr::curX = -1;
    draw(DrawContext(0, 0, Rect(0, 0, scr::cols, scr::rows)));
    scr::flush();
}

void
Application::destroy(View *v)
{
    if (v->owner)
        v->owner->remove(v);
    if (ndoomed < 16)
        doomed[ndoomed++] = v;
    else
        delete v;
}

void
Application::route(Event &e)
{
    Group *root = modal ? modal : this;
    if ((e.what & evMouse) && root != this && !root->mouseIn(e)) {
        return; /* outside a modal view */
    }
    root->handleEvent(e);
    if (e.what != evNothing && root != this && e.what == evCommand) {
        /* Commands a modal view left go nowhere else. */
        return;
    }
    while (ndoomed)
        delete doomed[--ndoomed];
}

void
Application::handleEvent(Event &e)
{
    Group::handleEvent(e);
    if (e.what == evCommand && e.command == cmQuit) {
        endModal(cmQuit);
        clearEvent(e);
    }
}

unsigned
Application::execView(Group *v)
{
    Group *saved = modal;
    if (v != this) {
        desktop->insert(v);
        v->select();
    }
    modal = v;
    v->endState = 0;
    while (!v->endState) {
        redraw();
        Event e;
        getEvent(e);
        route(e);
    }
    modal = saved;
    if (v != this)
        desktop->remove(v);
    return v->endState;
}

int
Application::run()
{
    execView(this);
    return 0;
}

void
Application::addWindow(Window *w)
{
    int n = 0;
    for (View *v = desktop->first; v; v = v->next)
        if (v->options & ofTopSelect)
            n++;
    if (w->bounds.x == 0 && w->bounds.y == 0) {
        w->bounds.x = n % 10 * 2;
        w->bounds.y = n % 10;
    }
    desktop->insert(w);
    w->select();
}

/* ---- Ready-made dialogs. ---- */

static int
wrappedLines(const char *s, int w)
{
    int lines = 0;
    while (*s) {
        int n = 0, cut = -1;
        while (s[n] && s[n] != '\n' && n < w) {
            if (s[n] == ' ')
                cut = n;
            n++;
        }
        if (s[n] && s[n] != '\n' && s[n] != ' ' && cut > 0)
            n = cut;
        s += n;
        if (*s == '\n' || *s == ' ')
            s++;
        lines++;
    }
    return lines;
}

unsigned
messageBox(const char *msg, unsigned flags)
{
    static const char *const titles[] = { "Warning", "Error", "Information", "Confirm" };
    static const char *const names[] = { "~Y~es", "~N~o", "O~K~", "Cancel" };
    static const unsigned cmds[] = { cmYes, cmNo, cmOK, cmCancel };
    int tw = 36;
    int lines = imax(1, wrappedLines(msg, tw));
    int nb = 0;
    for (int i = 0; i < 4; i++)
        if (flags & (0x100u << i))
            nb++;
    int w = imax(tw + 4, nb * 12 + 4), h = lines + 6;
    Dialog *d = new Dialog(Rect(0, 0, w, h), titles[flags & 3]);
    d->options |= ofCentered;
    d->insert(new StaticText(Rect(2, 1, w - 6, lines), msg));
    int x = (w - 2 - nb * 12 + 2) / 2;
    bool first = true;
    for (int i = 0; i < 4; i++) {
        if (!(flags & (0x100u << i)))
            continue;
        d->insert(new Button(Rect(x, h - 4, 10, 2), names[i], cmds[i], first));
        first = false;
        x += 12;
    }
    d->selectNext(false);
    unsigned r = app->execView(d);
    delete d;
    return r;
}

unsigned
inputBox(const char *title, const char *label, char *buf, int size)
{
    int w = 60, h = 9;
    Dialog *d = new Dialog(Rect(0, 0, w, h), title);
    d->options |= ofCentered;
    InputLine *in = new InputLine(Rect(3, 2, w - 8, 1), size - 1);
    in->setText(buf);
    d->insert(new Label(Rect(2, 1, w - 6, 1), label, in));
    d->insert(in);
    d->insert(new Button(Rect(w / 2 - 13, h - 5, 10, 2), "O~K~", cmOK, true));
    d->insert(new Button(Rect(w / 2 + 1, h - 5, 10, 2), "Cancel", cmCancel));
    in->select();
    unsigned r = app->execView(d);
    if (r == cmOK) {
        strncpy(buf, in->data, (size_t)size - 1);
        buf[size - 1] = 0;
    }
    delete d;
    return r;
}

} // namespace tui
