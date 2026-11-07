/* CXL compatible library: shared internals. Not installed.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef CXL_LOCAL_H
#define CXL_LOCAL_H

#include <i86.h>
#include <stdlib.h>
#include <string.h>

#include "cxldef.h"
#include "cxlvid.h"
#include "cxlkey.h"
#include "cxlmou.h"
#include "cxlwin.h"

/* Screen cells are char | attr << 8. */
#define CELL(ch, at) ((unsigned)(unsigned char)(ch) | ((unsigned)(unsigned char)(at) << 8))

/* video.c: every screen access goes through these, so BIOS output,
 * CGA snow avoidance and mono mapping work everywhere. */
unsigned _cxl_vget(int row, int col);
void     _cxl_vput(int row, int col, unsigned cell);
void     _cxl_vgetn(int row, int col, unsigned *dst, int n);
void     _cxl_vputn(int row, int col, const unsigned *src, int n);
/* Writes cells that came from the screen, without mapping them again. */
void     _cxl_vputraw(int row, int col, const unsigned *src, int n);
void     _cxl_vfill(int srow, int scol, int erow, int ecol, unsigned cell);
int      _cxl_attr(int attr); /* applies mono mapping when it is on */
void     _cxl_setcur(int row, int col);
void     _cxl_getcur(int *row, int *col);
/* Save and restore rectangles: a 4-int header (srow, scol, erow, ecol)
 * followed by the cells. */
int     *_cxl_save(int srow, int scol, int erow, int ecol);
void     _cxl_restore(const int *buf);
void     _cxl_restore_at(const int *buf, int srow, int scol);

/* box.c */
extern const unsigned char _cxl_boxch[6][7]; /* tl tr bl br top bottom side */
void     _cxl_box(int srow, int scol, int erow, int ecol, int btype, int attr);

/* keyboard.c */
extern int (*_cxl_helphook)(unsigned key); /* help hot key, set by whelpdef */
unsigned _cxl_biosget(void);
int      _cxl_bioshit(void);

/* mouse.c: polls the mouse while getxch() waits; returns a key or 0. */
unsigned _cxl_mousekey(void);
extern int _cxl_mrow, _cxl_mcol; /* where the last left click was */

/* window.c */
int      _cxl_werr(int code);              /* sets _winfo.errno, returns it */
int      _cxl_wbeg(void);                  /* W_NOACTIVE check */
int      _cxl_wrow0(struct _wrec_t *w);    /* first screen row inside */
int      _cxl_wcol0(struct _wrec_t *w);
int      _cxl_wrows(struct _wrec_t *w);    /* inside size */
int      _cxl_wcols(struct _wrec_t *w);
void     _cxl_wsyncur(void);               /* screen cursor to window cursor */
void     _cxl_wdrawframe(struct _wrec_t *w);
struct _wrec_t *_cxl_wfind(WINDOW h, int *hidden);
void     _cxl_wlift(struct _wrec_t *w);    /* take the top window off screen */
void     _cxl_wplace(struct _wrec_t *w);   /* put a lifted window back */
void     _cxl_wfree(struct _wrec_t *w);
/* Runs fn with w as the active window, lifting the windows above it. */
int      _cxl_wwith(struct _wrec_t *w, int (*fn)(void *), void *arg);

/* wout.c */
void     _cxl_wtty(int ch);                /* TTY output, active window */
void     _cxl_wttys(const char *s, int wrap); /* with escape codes */

/* format.c: the CXL input format language (Appendix D of the manual). A
 * format is parsed into slots: one per input position or literal
 * character. */
#define SL_INPUT   0 /* input position of character class ch */
#define SL_INCL    1 /* input position limited to set[0..setlen) */
#define SL_EXCL    2 /* input position excluding set[0..setlen) */
#define SL_LIT     3 /* literal text character ch */
#define SL_DECIMAL 4 /* decimal point of a numeric field (forms only) */

#define SF_COPY  0x01 /* copy literals into the result */
#define SF_ESC   0x02 /* Esc cancels */
#define SF_LOWER 0x04
#define SF_MIXED 0x08
#define SF_PASS  0x10 /* echo spaces */
#define SF_RET   0x20 /* Enter accepted here */
#define SF_UPPER 0x40

struct _cxl_slot {
    unsigned char kind;
    unsigned char ch;
    unsigned char flags;
    signed char attrd;     /* attribute change from !+! and !-! */
    const char *set;
    unsigned char setlen;
};

/* Parses fmt. toggles allows !..! sequences; flags gives the starting
 * toggle state. Returns the slot count, or -1 for a bad format, and
 * stores a malloc'd array in *out (NULL if count is 0). */
int  _cxl_fmtparse(const char *fmt, int toggles, int flags,
                   struct _cxl_slot **out);
/* Whether ch may go into the slot; returns the converted character or
 * -1. prev is the input character before it, or 0. */
int  _cxl_slotok(const struct _cxl_slot *s, int ch, int prev, int fconv);

/* The single-line formatted input engine behind inputsf(), winputsf(),
 * getns() and wgetns(). */
struct _cxl_lineio {
    void (*getpos)(int *row, int *col);
    void (*setpos)(int row, int col);
    void (*tty)(int ch, int attr);
    void (*putat)(int row, int col, int ch, int attr);
    int attr;
};
/* Returns 0, 1 for Esc, 2 for a bad format. */
int  _cxl_lineinput(const struct _cxl_lineio *io, char *str, const char *fmt,
                    int escflag, int toggles);
/* Plain string input of at most maxchars characters (maxchars < 0 for no
 * limit). Returns 0 or 1 for Esc. */
int  _cxl_strinput(const struct _cxl_lineio *io, char *str, int maxchars,
                   int esc);

/* form.c */
extern char *_cxl_wordsep;

/* help.c */
void     _cxl_helpkey_install(void);

#endif /* CXL_LOCAL_H */
