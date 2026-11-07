/* CXL compatible library: keyboard functions. The window keyboard
 * functions are in <cxlwin.h>.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __CXLKEY_H
#define __CXLKEY_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _Cdecl
#define _Cdecl
#endif
#ifndef _Near
#define _Near
#endif

/* A key waiting in CXL's own key buffer (kbput, kbputs). */
struct _kbuf_t {
    struct _kbuf_t *prev;
    struct _kbuf_t *next;
    unsigned        xch;
};

/* A key attached to a function by setonkey(). */
struct _onkey_t {
    struct _onkey_t *prev;
    struct _onkey_t *next;
    unsigned int     keycode; /* scan/ASCII code of the key */
    void (*func)(void);       /* function to call */
    unsigned int pass;        /* key to return afterwards, 0 for none */
};

/* The keyboard information record. */
struct _kbinfo_t {
    struct _kbuf_t  *kbuf;  /* head of the key buffer */
    struct _onkey_t *onkey; /* head of the onkey list */
    void (*kbloop)(void);   /* called while waiting for a key */
    unsigned char inmenu;   /* set while a menu or pick list runs */
    unsigned char source;   /* last key: 0 keyboard, 1 buffer, 2 mouse */
};
extern struct _kbinfo_t _kbinfo;

/* kbstat() bits. */
#define RSHIFT   1
#define LSHIFT   2
#define CTRL     4
#define ALT      8
#define SCRLOCK  16
#define NUMLOCK  32
#define CAPSLOCK 64
#define INS      128

#define setkbloop(a) (_kbinfo.kbloop = (a))

void             capsoff(void);
void             capson(void);
struct _onkey_t *chgonkey(struct _onkey_t *kblist);
void             clearkeys(void);
void             freonkey(void);
int              getchf(char *valid, int defchar);
int              getns(char *str, int maxchars);
unsigned         getxch(void);
int              inputsf(char *str, char *fmt);
void             kbclear(void);
int              kbmhit(void);
int              kbput(unsigned xch);
int              kbputs(char *str);
unsigned         kbstat(void);
void             numoff(void);
void             numon(void);
int              scancode(int ch);
int              setonkey(unsigned keycode, void (*func)(void), unsigned pass);
int              waitkey(void);
int              waitkeyt(int duration);

#ifdef __cplusplus
}
#endif

#endif /* __CXLKEY_H */
