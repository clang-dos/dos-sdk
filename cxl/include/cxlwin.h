/* CXL compatible library: windows, menus, data-entry forms, help and
 * pick lists.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __CXLWIN_H
#define __CXLWIN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _Cdecl
#define _Cdecl
#endif
#ifndef _Near
#define _Near
#endif

typedef int WINDOW;

/* Error codes left in _winfo.errno and returned by most functions. */
#define W_NOERROR   0  /* no error */
#define W_ESCPRESS  1  /* the user pressed Esc */
#define W_ALLOCERR  2  /* out of memory */
#define W_NOTFOUND  3  /* no such record */
#define W_NOACTIVE  4  /* no window is open */
#define W_INVCOORD  5  /* coordinates out of range */
#define W_INVFORMT  6  /* bad format string */
#define W_NOINPDEF  7  /* no input fields were defined */
#define W_STRLONG   8  /* string does not fit */
#define W_INVBTYPE  9  /* bad box type */
#define W_NOBORDER  10 /* the window has no border */
#define W_NOHIDDEN  11 /* no window is hidden */
#define W_NOTHIDD   12 /* the window is not hidden */
#define W_NOSELECT  13 /* no menu item can be selected */
#define W_NOITMDEF  14 /* no menu items were defined */
#define W_NOMNUEND  15 /* wmenuend() is missing */
#define W_NOMNUDEF  16 /* no menu was defined */
#define W_NOMNUBEG  17 /* wmenubeg() is missing */
#define W_NOFRMDEF  18 /* no form was defined */
#define W_NOFRMBEG  19 /* winpbeg() is missing */
#define W_NOHLPDEF  20 /* whelpdef() was not called */
#define W_HLPSTKOV  21 /* help stack is full */
#define W_HLPSTKUN  22 /* help stack is empty */
#define W_DOSERROR  23 /* a DOS call failed */
#define W_NOMATCH   24 /* no file matched the file spec */
#define W_INVTAGID  25 /* bad or duplicate tag id */
#define W_INVPATH   26 /* bad path (TCXL) */

/* Borders, for wbprintc(). */
#define TP_BORD 0
#define BT_BORD 1
#define LT_BORD 2
#define RT_BORD 3

/* Directions, for wscroll(), wdelline(), winsline(), wdrag(). */
#define D_DOWN  0
#define D_UP    1
#define D_LEFT  2
#define D_RIGHT 3
#define SDOWN   D_DOWN
#define SUP     D_UP

/* Title positions, for wtitle(). */
#define TLEFT   1
#define TCENTER 2
#define TRIGHT  3

/* wmenuitem() feature mask. */
#define M_HASPD 1  /* a pull-down menu hangs off this item */
#define M_NOSEL 2  /* the item cannot be selected */
#define M_CLOSE 4  /* close the menu after the select function returns */
#define M_CLALL 8  /* close all menus when the item is selected */
#define M_CLOSB 16 /* close the menu before calling the select function */

/* wmenuend() menu type. */
#define M_HORZ 1  /* horizontal: left and right arrows move */
#define M_VERT 2  /* vertical: up and down arrows move */
#define M_OMNI 7  /* all four arrows move */
#define M_PD   8  /* a pull-down menu */
#define M_NOQS 16 /* no quick selection by letter */
#define M_SAVE 32 /* remember the bar position between calls */

/* Special results of a menu. */
#define M_EXIT    32764 /* leave this menu */
#define M_EXITALL 32765 /* leave all menus */
#define M_PREVPD  32766 /* move to the previous pull-down */
#define M_NEXTPD  32767 /* move to the next pull-down */

struct _menu_t;

/* A menu item. */
struct _item_t {
    struct _item_t *prev;
    struct _item_t *next;
    void *child;             /* submenu (struct _menu_t *) */
    char *str;               /* item text */
    char *desc;              /* description shown while the bar is on it */
    void (*select)(void);    /* called when the item is selected */
    void (*before)(void);    /* called when the bar moves onto it */
    void (*after)(void);     /* called when the bar leaves it */
    unsigned hotkey;         /* key that selects it from anywhere */
    int tagid;               /* tag id */
    int help;                /* help category */
    int wrow;                /* row in the menu window */
    unsigned char wcol;      /* column in the menu window */
    unsigned char schar;     /* quick-select character */
    unsigned char fmask;     /* M_HASPD, M_NOSEL, ... */
    unsigned char dwrow;     /* where the description goes */
    unsigned char dwcol;
    unsigned char dattr;     /* description attribute */
    unsigned char redisp;    /* item needs to be redrawn */
};

/* A menu. */
struct _menu_t {
    struct _menu_t *prev;
    struct _menu_t *next;
    struct _menu_t *parent;
    struct _item_t *item;    /* first item */
    struct _item_t *citem;   /* item under the bar */
    void (*open)(void);      /* called after the menu window opens */
    int tagcurr;             /* tag of the item under the bar */
    unsigned char usecurr;   /* menu lives in the current window */
    unsigned char srow;
    unsigned char scol;
    unsigned char erow;
    unsigned char ecol;
    unsigned char btype;
    unsigned char battr;
    unsigned char wattr;
    unsigned char menutype;
    unsigned char barwidth;  /* 0 means the width of the text */
    unsigned char textpos;   /* text offset within the bar */
    unsigned char textattr;
    unsigned char scharattr; /* quick-select character attribute */
    unsigned char noselattr; /* unselectable item attribute */
    unsigned char barattr;
    /* Library private. */
    int _taginit;
    WINDOW _win;             /* window while the menu is open */
    unsigned char _ended;    /* wmenuend() was called */
};

/* Help system settings, set up by whelpdef() and whelpwin(). */
struct _help_t {
    int help[20];            /* help category stack */
    char *file;              /* help file */
    int helpptr;             /* stack pointer, -1 when empty */
    unsigned int key;        /* help hot key */
    unsigned char winattr;
    unsigned char textattr;
    unsigned char selattr;   /* cross-reference attribute */
    unsigned char barattr;   /* selected cross-reference attribute */
    unsigned char srow;
    unsigned char scol;
    unsigned char erow;
    unsigned char ecol;
    unsigned char btype;
    unsigned char title;     /* show a "Help" title */
    void (*open)(void);      /* called after the help window opens */
};

/* Global window state. */
struct _winfo_t {
    struct _wrec_t *active;  /* the active window (top of the stack) */
    struct _wrec_t *hidden;  /* most recently hidden window */
    struct _menu_t *menu;    /* menu being defined or run */
    struct _menu_t *cmenu;   /* current menu */
    struct _help_t *helptr;  /* help settings */
    WINDOW handle;           /* last handle given out */
    int maxfiles;            /* most files wpickfile() will list */
    int help;                /* current help category */
    int errno;               /* error code of the last window call */
    int total;               /* open windows */
    int mlevel, ilevel;      /* menu nesting levels */
    unsigned char esc;       /* Esc cancels input */
    unsigned char tabwidth;  /* tab width for window TTY output */
    unsigned char fillch;    /* character new windows are filled with */
};
extern struct _winfo_t _winfo;

/* An input field. */
struct _field_t {
    struct _field_t *prev;
    struct _field_t *next;
    char *str;               /* caller's string */
    char *buf;               /* edit buffer */
    char *format;            /* format string */
    int (*validate)(char *); /* returns 0, or the 1-based bad position */
    void (*before)(void);
    void (*after)(void);
    int help;
    int lenbuf;              /* characters that can be entered */
    int lenfld;              /* screen width of the field */
    int lenfmt;
    unsigned char wrow;
    unsigned char wcol;
    unsigned char mode;      /* 0 init, 1 update, 2 conditional update */
    unsigned char decpos;    /* decimal point position, numeric fields */
    unsigned char redisp;
    char fconv;              /* conversion: 0, L, M, P, U or 9 */
    /* Library private. */
    void *_slots;            /* the parsed format */
    int _nslots;
    char *_orig;             /* caller's string before winpread() */
    int _tag;                /* TCXL field tag */
};

/* A data-entry form. */
struct _form_t {
    unsigned (*getkey)(int *); /* alternate key function (winpkey) */
    struct _form_t *prev;
    struct _form_t *next;
    struct _field_t *field;  /* first field */
    struct _field_t *cfield; /* field being edited */
    unsigned *termkey;       /* receives the key that ended the form */
    char *pformat;
    char *pbuf;
    unsigned char cwrow;
    unsigned char cwcol;
    unsigned char decimal;
    unsigned char insert;    /* insert mode */
    unsigned char fieldattr;
    unsigned char textattr;
};

/* A window. */
struct _wrec_t {
    struct _wrec_t *prev;
    struct _wrec_t *next;
    struct _form_t *form;
    int *wbuf;               /* screen under the window */
    int *wsbuf;              /* screen under the shadow */
    char *title;
    WINDOW whandle;
    int help;
    unsigned char srow;
    unsigned char scol;
    unsigned char erow;
    unsigned char ecol;
    unsigned char btype;
    unsigned char wattr;
    unsigned char battr;
    unsigned char border;    /* 1 if the window has a border */
    unsigned char row;       /* cursor, window relative */
    unsigned char column;
    unsigned char attr;      /* current text attribute */
    unsigned char tpos;
    unsigned char tattr;
    unsigned char wsattr;    /* shadow attribute */
    /* Library private. */
    int *_cbuf;              /* window contents while it is hidden or lifted */
    unsigned char _shadow;   /* 1 if the window casts a shadow */
    unsigned char _hidden;   /* 1 while it is hidden */
};

#define wclear()      wcclear(_winfo.active->wattr)
#define wfillch(a)    (_winfo.fillch = (a))
#define whelpundef()  whelpdef(NULL, 0, 0, 0, 0, 0, NULL)
#define winpfcurr()   (_winfo.active->form->cfield)
#define wisactiv(a)   ((a) == _winfo.active->whandle)
#define isactiv(a)    wisactiv(a)
#define wmenumcurr()  (_winfo.cmenu)
#define wmenuicurr()  (wmenumcurr()->citem)
#define wsetesc(a)    (_winfo.esc = (a))
#define wtabwidth(a)  (_winfo.tabwidth = (((a) == 0) ? 1 : (a)))

int     wactiv(WINDOW whandle);
int     wborder(int btype);
int     wbox(int wsrow, int wscol, int werow, int wecol, int btype, int attr);
int     wbprintc(int bord, int offs, int attr, int ch);
int     wcclear(int attr);
int     wcenters(int wrow, int attr, char *str);
int     wchgattr(int battr, int wattr);
int     wchkbox(int wsrow, int wscol, int werow, int wecol);
int     wchkcol(int wcol);
int     wchkcoord(int wrow, int wcol);
int     wchkrow(int wrow);
int     wclose(void);
int     wcloseall(void);
int     wclreol(void);
int     wclreos(void);
WINDOW  wcopy(int nsrow, int nscol);
int     wdelline(int wrow, int direc);
int     wdrag(int direction);
int     wdump(void);
int     wdupc(int ch, int count);
int     wdups(char *str, int count);
char   *werrmsg(void);
int     wfill(int wsrow, int wscol, int werow, int wecol, int ch, int attr);
struct _wrec_t *wfindrec(WINDOW whandle);
int     wgetc(void);
int     wgetchf(char *valid, int defchar);
int     wgetns(char *str, int maxchars);
int     wgets(char *str);
int     wgetyn(int cdefault);
int     wgotoxy(int wrow, int wcol);
WINDOW  whandle(void);
int     whelpcat(int cat);
int     whelpclr(void);
int     whelpdef(char *file, unsigned key, int winattr, int textattr,
                 int selattr, int barattr, void (*open)(void));
int     whelpget(void);
int     whelpop(void);
int     whelpopc(void);
int     whelppcat(int cat);
int     whelpush(void);
int     whelpushc(int cat);
int     whelpwin(int srow, int scol, int erow, int ecol, int btype, int title);
int     whide(void);
int     whline(int wsrow, int wscol, int count, int btype, int attr);
WINDOW  windowat(int row, int col);
int     winpbeg(int fieldattr, int textattr);
int     winpdef(int wrow, int wcol, char *str, char *format, int fconv,
                int mode, int (*validate)(char *), int help);
int     winpfba(void (*before)(void), void (*after)(void));
struct _field_t *winpffind(int wrow, int wcol);
int     winpkey(unsigned (*getkey)(int *), unsigned *termkey);
int     winpread(void);
int     winputsf(char *str, char *fmt);
int     winpwordsep(char *sepstr);
int     winsline(int wrow, int direc);
int     wmenubeg(int srow, int scol, int erow, int ecol, int btype,
                 int battr, int wattr, void (*open)(void));
int     wmenubegc(void);
int     wmenuend(int taginit, int menutype, int barwidth, int textpos,
                 int textattr, int scharattr, int noselattr, int barattr);
int     wmenuget(void);
int     wmenuiba(void (*before)(void), void (*after)(void));
int     wmenuidsab(int tagid);
int     wmenuienab(int tagid);
struct _item_t *wmenuifind(int tagid);
char   *wmenuigetxt(int tagid);
int     wmenuinext(int tagid);
int     wmenuiredisp(int tagid);
int     wmenuisetxt(int tagid, char *str);
int     wmenuitem(int wrow, int wcol, char *str, int schar, int tagid,
                  int fmask, void (*select)(void), unsigned hotkey, int help);
int     wmenuitxt(int wrow, int wcol, int attr, char *str);
int     wmessage(char *str, int border, int leftofs, int attr);
int     wmove(int nsrow, int nscol);
WINDOW  wopen(int srow, int scol, int erow, int ecol, int btype, int battr,
              int wattr);
int     wperror(char *message);
int     wpgotoxy(int wrow, int wcol);
int     wpickattr(int srow, int scol, int btype, int initattr, int blink,
                  void (*open)(void));
char   *wpickfile(int srow, int scol, int erow, int ecol, int btype,
                  int bordattr, int winattr, int barattr, int title,
                  char *filespec, void (*open)(void));
int     wpickstr(int srow, int scol, int erow, int ecol, int btype,
                 int bordattr, int winattr, int barattr, char *strarr[],
                 int initelem, void (*open)(void));
int     wprintc(int wrow, int wcol, int attr, int ch);
int     wprintf(const char *format, ...) __attribute__((__format__(__printf__, 1, 2)));
int     wprints(int wrow, int wcol, int attr, char *str);
int     wprintsf(int wrow, int wcol, int attr, char *format, char *str);
int     wputc(int ch);
int     wputns(char *str, int width);
int     wputs(char *str);
int     wputsw(char *str);
int     wreadcur(int *wrow, int *wcol);
void    wrestore(int *wbuf);
int     wrjusts(int wrow, int wjcol, int attr, char *str);
int    *wsave(int srow, int scol, int erow, int ecol);
int     wscanf(const char *format, ...);
int     wscroll(int count, int direc);
int     wscrollbox(int wsrow, int wscol, int werow, int wecol, int count,
                   int direction);
int     wselstr(int wrow, int wcol, int attr, char *strarr[], int initelem);
int     wsetfill(void (*fill)(unsigned, unsigned, int, int));
int     wshadoff(void);
int     wshadow(int attr);
int     wsize(int nerow, int necol);
int     wslide(int nsrow, int nscol);
int     wtextattr(int attr);
int     wtitle(char *str, int tpos, int tattr);
int     wunhide(WINDOW whandle);
int     wunlink(WINDOW whandle);
int     wvline(int wsrow, int wscol, int count, int btype, int attr);
int     wwprints(WINDOW whandle, int wrow, int wcol, int attr, char *str);

/* CXL 5.0 had no initelem argument for wpickstr() and wselstr(). These
 * macros accept calls written either way; the 5.0 form starts at element 0.
 * Taking the functions' addresses is not affected. Define CXL_NO_50_CALLS
 * to turn them off. */
#ifndef CXL_NO_50_CALLS
static inline int _cxl_wpickstr50(int srow, int scol, int erow, int ecol,
                                  int btype, int bordattr, int winattr,
                                  int barattr, char *strarr[],
                                  void (*open)(void))
{
    return (wpickstr)(srow, scol, erow, ecol, btype, bordattr, winattr,
                      barattr, strarr, 0, open);
}
static inline int _cxl_wselstr50(int wrow, int wcol, int attr, char *strarr[])
{
    return (wselstr)(wrow, wcol, attr, strarr, 0);
}
#define _CXL_ARG12(a, b, c, d, e, f, g, h, i, j, k, n, ...) n
#define _CXL_ARG6(a, b, c, d, e, n, ...) n
#define wpickstr(...) \
    _CXL_ARG12(__VA_ARGS__, (wpickstr), _cxl_wpickstr50, )(__VA_ARGS__)
#define wselstr(...) \
    _CXL_ARG6(__VA_ARGS__, (wselstr), _cxl_wselstr50, )(__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __CXLWIN_H */
