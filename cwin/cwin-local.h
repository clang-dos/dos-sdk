/* The insides of the character window system.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef CWIN_LOCAL_H
#define CWIN_LOCAL_H

#include "cwin.h"
#include <stdint.h>

#define WM_SYSKEYUP 0x0105

/* Objects behind the handles. */
enum { OBJ_FREE, OBJ_WND, OBJ_MENU, OBJ_ACCEL };

struct cls {
    struct cls *next;
    WNDCLASS wc;
    char *name;
};

struct wnd {
    BYTE kind;
    HWND hwnd;
    struct cls *cls;
    WNDPROC proc;
    DWORD style, exstyle;
    UINT id;
    HMENU menu;
    struct wnd *parent, *owner;
    struct wnd *first, *last, *next, *prev; /* children, bottom to top */
    struct wnd *lastFocus;                  /* top-level: its focused child */
    int x, y, w, h;                         /* in the parent's client area */
    RECT normal;                            /* the place before zooming */
    BYTE zoomed, destroyed, attr;
    char *text;
    LONG userdata;
    int sbmin, sbmax, sbpos;
    /* The drawing context while painting: origin and clip, in screen cells. */
    int dcx, dcy;
    RECT dcclip;
    /* Dialogs. */
    DLGPROC dlgproc;
    LONG dlgresult, dlguser;
    int endResult;
    BYTE ended, isDialog;
    void *ctl; /* the data of a control class */
    char extra[1];
};

struct menuitem {
    UINT flags;
    UINT id;      /* or the popup's handle */
    char *text;
};

struct menu {
    BYTE kind;
    HMENU hmenu;
    BYTE popup;
    int n, cap;
    struct menuitem *items;
};

struct accel {
    BYTE kind;
    HACCEL hacc;
    int n;
    ACCEL t[1];
};

/* Raw input. */
enum { EV_NONE, EV_KEY, EV_DOWN, EV_UP, EV_MOVE, EV_ALTTAP };
struct cwev {
    BYTE type;
    BYTE shift;   /* INT 16h shift flags */
    BYTE buttons; /* 1 left, 2 right */
    BYTE dbl;
    WORD key;     /* scan << 8 | character */
    int x, y;
};

/* objects.c */
void *cw_obj(HANDLE h, int kind);
HANDLE cw_newhandle(void *obj);
void cw_freehandle(HANDLE h);
#define W(h) ((struct wnd *)cw_obj((h), OBJ_WND))
#define M(h) ((struct menu *)cw_obj((h), OBJ_MENU))
char *cw_strdup(const char *s);
int cw_imin(int a, int b);
int cw_imax(int a, int b);
int cw_upper(int c);
/* The mnemonic letter after a & (upper case), or 0; the length without
 * the &s. */
int cw_mnemonic(const char *s);
int cw_textlen(const char *s);

/* screen.c */
extern int cw_cols, cw_rows;
extern uint16_t *cw_buf;
extern BYTE cw_mouse;
extern int cw_mx, cw_my;
extern BYTE cw_inited;
extern BYTE cw_sys[CW_NCOLORS];
void cw_init(void);
void cw_flush(int curx, int cury);
int cw_poll(struct cwev *e);
void cw_idle(void);
uint16_t cw_ticks(void);
BYTE cw_shiftflags(void);
void cw_put(const RECT *clip, int x, int y, BYTE ch, BYTE attr);
void cw_fill(const RECT *clip, int x, int y, int w, int h, BYTE ch, BYTE attr);
void cw_shade(const RECT *clip, int x, int y, int w, int h);
int cw_text(const RECT *clip, int x, int y, const char *s, int n, BYTE attr, BYTE hot);
void cw_box(const RECT *clip, int x, int y, int w, int h, BYTE attr, BOOL dbl);
void cw_rect(RECT *r, int x, int y, int w, int h);
void cw_intersect(RECT *r, const RECT *a);

/* window.c */
extern struct wnd *cw_desktop, *cw_focus, *cw_active, *cw_capture;
extern char *cw_status;
extern BYTE cw_dirty;
void cw_setup(void);
struct wnd *cw_top(struct wnd *w);
/* The window's rectangle and its client area, in screen cells. */
void cw_winrect(struct wnd *w, RECT *r);
void cw_clientrect(struct wnd *w, RECT *r);
/* The client area in window coordinates (from the window's corner). */
void cw_clientinset(struct wnd *w, RECT *r);
int cw_hasframe(struct wnd *w);
void cw_paintall(void);
LRESULT cw_send(struct wnd *w, UINT msg, WPARAM wp, LPARAM lp);
void cw_post(struct wnd *w, UINT msg, WPARAM wp, LPARAM lp);
void cw_dispatch_input(const struct cwev *e);
/* Wait for raw input, repainting first (for modal loops). */
void cw_wait_input(struct cwev *e);
BYTE cw_ctlattr(struct wnd *w);
int cw_vkey(WORD key);
int cw_isdlg(struct wnd *w);
void cw_scrollbar_rect(struct wnd *w, RECT *r);

/* menu.c */
void cw_menubar_paint(struct wnd *w, const RECT *clip);
int cw_menubar_row(struct wnd *w);   /* screen row, or -1 */
int cw_menubar_hit(struct wnd *w, int x, int y);
void cw_menu_overlay(void);
void cw_track_bar(struct wnd *w, int item, BOOL open, BOOL mouse);
int cw_menu_findkey(struct wnd *w, int c);

/* controls.c */
void cw_register_controls(void);

/* dialog.c */
LRESULT cw_dialogproc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
extern const char cw_dialogclass[];

#endif
