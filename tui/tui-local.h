/* The screen, keyboard and mouse under the tui views.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef TUI_LOCAL_H
#define TUI_LOCAL_H

#include "tui.h"
#include <stdint.h>

namespace tui {
namespace scr {
extern int cols, rows;
extern uint16_t *buf;     /* rows * cols cells: attribute << 8 | character */
extern int curX, curY;    /* where to show the text cursor; curX < 0 hides it */
extern bool mouse;        /* a mouse driver is there */
extern int mx, my;        /* the mouse, in cells */
void init();
void done();
/* Copy the buffer to the screen, with the mouse cell inverted. */
void flush();
/* An event from the keyboard or the mouse, if there is one. */
bool poll(Event &e);
/* Give the processor away while idle (DOS idle call, Windows, emulators). */
void release();
} // namespace scr

/* Small string helpers. */
char *dupstr(const char *s);
inline int imin(int a, int b) { return a < b ? a : b; }
inline int imax(int a, int b) { return a > b ? a : b; }
} // namespace tui

#endif
