/* CXL compatible library: Microsoft compatible mouse functions.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __CXLMOU_H
#define __CXLMOU_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _Cdecl
#define _Cdecl
#endif
#ifndef _Near
#define _Near
#endif

/* The mouse support level, set by msinit() and mssupport(). */
extern int _mouse;

#define MS_NONE 0 /* no mouse support */
#define MS_KEYS 1 /* mouse movement emulates the arrow keys */
#define MS_CURS 2 /* free-moving mouse cursor where supported */
#define MS_FULL 3 /* both */

void msbclear(void);
void msbpress(int button, int *bstat, int *bcount, int *row, int *col);
void msbreles(int button, int *bstat, int *bcount, int *row, int *col);
void mscondoff(int srow, int scol, int erow, int ecol);
void mscursor(unsigned curtype, unsigned smask, unsigned cmask);
void msgotoxy(int row, int col);
void mshbounds(int leftcol, int rightcol);
void mshidecur(void);
int  msinit(void);
void msmotion(int *rowcount, int *colcount);
void msshowcur(void);
void msspeed(int xratio, int yratio);
void msstatus(int *bstat, int *row, int *col);
void mssupport(int type);
void msvbounds(int toprow, int botrow);

#ifdef __cplusplus
}
#endif

#endif /* __CXLMOU_H */
