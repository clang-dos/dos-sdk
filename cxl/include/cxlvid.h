/* CXL compatible library: screen and video functions.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __CXLVID_H
#define __CXLVID_H

#include <i86.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _Cdecl
#define _Cdecl
#endif
#ifndef _Near
#define _Near
#endif

/* Display adapters returned by vidtype() and videoinit(). */
#define V_NONE     0
#define V_MDA      1
#define V_EGAMONO  2
#define V_MCGAMONO 3
#define V_VGAMONO  4
#define V_HGC      5
#define V_HGCPLUS  6
#define V_INCOLOR  7
#define V_CGA      8
#define V_EGA      9
#define V_MCGA     10
#define V_VGA      11

/* setvparam() settings. */
#define VP_DMA   0 /* direct screen writes */
#define VP_CGA   1 /* direct screen writes without CGA snow */
#define VP_BIOS  2 /* screen writes through the video BIOS */
#define VP_MONO  3 /* map color attributes to monochrome */
#define VP_COLOR 4 /* do not map color attributes */

/* The video information record, set up by videoinit(). */
struct _vinfo_t {
    unsigned int  videoseg; /* video buffer segment */
    unsigned char adapter;  /* display adapter, V_ */
    unsigned char numrows;  /* rows on the screen */
    unsigned char numcols;  /* columns on the screen */
    unsigned char cheight;  /* character height in pixels */
    unsigned char cwidth;   /* character width in pixels */
    unsigned char mono;     /* monochrome adapter? */
    unsigned char mapattr;  /* map color attributes to monochrome? */
    unsigned char cgasnow;  /* avoid CGA snow? */
    unsigned char usebios;  /* write through the BIOS? */
    unsigned char dvcheck;  /* look for DESQview in videoinit()? */
    unsigned char dvexist;  /* is DESQview running? */
};
extern struct _vinfo_t _vinfo;

/* Text attributes. The foreground colors share <conio.h>'s enum. */
#ifndef __COLORS
#define __COLORS
enum COLORS {
    BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY,
    DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA,
    YELLOW, WHITE
};
#endif
#ifndef LGREY
#define LGREY    7
#define DGREY    8
#define LBLUE    9
#define LGREEN   10
#define LCYAN    11
#define LRED     12
#define LMAGENTA 13
#define _BLACK   0
#define _BLUE    16
#define _GREEN   32
#define _CYAN    48
#define _RED     64
#define _MAGENTA 80
#define _BROWN   96
#define _LGREY   112
#endif
#define BLINK 128

#define attrib(f, b, i, bl) (((b) << 4) | (f) | ((i) << 3) | ((bl) << 7))

char    *attr2text(int attr);
void     box_(int srow, int scol, int erow, int ecol, int btype, int attr);
void     cclrscrn(int attr);
int      cgardbyte(char __far *src);
int      cgardword(int __far *src);
void     cgareadn(int __far *src, int *dest, unsigned n);
void     cgawrbyte(char __far *dest, int chr);
void     cgawriten(int *src, int __far *dest, unsigned n);
void     cgawrstr(char __far *dest, char *string, int attr);
void     cgawrword(int __far *dest, int chratr);
void     clreol_(void);
void     clrscrn(void);
void     clrwin(int srow, int scol, int erow, int ecol);
int      disktoscrn(char *fname);
int      disktowin(char *fname);
void     fill_(int srow, int scol, int erow, int ecol, int ch, int attr);
void     getcursz(int *sline, int *eline);
void     gotoxy_(int row, int col);
void     hidecur(void);
void     lgcursor(void);
int      mapattr(int attr);
void     mode(int mode_code);
void     printc(int row, int col, int attr, int ch);
void     prints(int row, int col, int attr, char *str);
void     putchat(int ch, int attr);
unsigned readchat(void);
void     readcur(int *row, int *col);
void     revattr(int count);
int      revsattr(int attr);
int      scrntodisk(char *fname);
void     setattr(int attr, int count);
void     setcursz(int sline, int eline);
int      setlines(int numlines);
int      setvparam(int setting);
void     showcur(void);
void     smcursor(void);
void     spc(int num);
void     srestore(int *sbuf);
int     *ssave(void);
int      videoinit(void);
int      vidmode(void);
int      vidtype(void);
int      wintodisk(int srow, int scol, int erow, int ecol, char *fname);

#ifdef __cplusplus
}
#endif

#endif /* __CXLVID_H */
