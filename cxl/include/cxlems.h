/* CXL compatible library: expanded memory (LIM EMS) functions.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __CXLEMS_H
#define __CXLEMS_H

#ifdef __cplusplus
extern "C" {
#endif

int      emsalloc(int numpages);
int      emsdealloc(int handle);
int      emsexist(void);
unsigned emsframe(void);
unsigned emsfree(void);
int      emsmap(int handle, int lpage, int ppage);
int      emsread(char *dest, unsigned emsofs, unsigned numbytes);
unsigned emstotal(void);
char    *emsver(void);
int      emswrite(char *src, unsigned emsofs, unsigned numbytes);

#ifdef __cplusplus
}
#endif

#endif /* __CXLEMS_H */
