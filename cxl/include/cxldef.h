/* CXL compatible library: miscellaneous functions and common definitions.
 *
 * A new implementation of the interface of the CXL 5.2 C library (Mike
 * Smedley, 1987-1990), written from its published reference manual.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __CXLDEF_H
#define __CXLDEF_H

#include <stddef.h>
#include <i86.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The calling convention and near-data keywords of the original headers. */
#ifndef _Cdecl
#define _Cdecl
#endif
#ifndef _Near
#define _Near
#endif

/* Common control codes. */
#ifndef NUL
#define NUL 0
#endif
#define BEL 7
#define BS  8
#define HT  9
#define LF  10
#define FF  12
#define CR  13
#define ESC 27

/* Boolean conditions. */
#ifndef NO
#define NO 0
#endif
#ifndef YES
#define YES 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef OFF
#define OFF 0
#endif
#ifndef ON
#define ON 1
#endif

/* Machine ID bytes returned by machid(). */
#define IBMPC    255
#define IBMPCXT  254
#define IBMPCJR  253
#define IBMPCAT  252
#define IBMPCXT2 251
#define IBMCONV  249
#define SPERRYPC 48

/* sysdate() and systime() take a format number from 0 to 5. */

void          beep(void);
char         *biosver(void);
int           clockcal(void);
char         *cxlver(void);
void          delay_(unsigned duration);
unsigned      expmem(void);
unsigned      extmem(void);
int           fcrypt(char *file, char *key);
int           gameport(void);
int           machid(void);
int           mathchip(void);
int           numflop(void);
int           numpar(void);
int           numser(void);
char         *randfile(void);
void          sound_(unsigned pitch, unsigned duration);
char         *sysdate(int dtype);
char         *systime(int ttype);
int           tabstop(int col, int tabwidth);
unsigned long timer(void);
void          _stdoutch(int ch);

#ifdef __cplusplus
}
#endif

#endif /* __CXLDEF_H */
