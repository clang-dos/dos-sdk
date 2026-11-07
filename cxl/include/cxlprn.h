/* CXL compatible library: printer functions. Output goes to PRN.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __CXLPRN_H
#define __CXLPRN_H

#ifdef __cplusplus
extern "C" {
#endif

void lcrlf(void);
void lprintc(int ch);
void lprintf(const char *format, ...) __attribute__((__format__(__printf__, 1, 2)));
void lprintns(char *str, int width);
void lprints(char *str);
void lprintsb(char *str, int reps);
void lprintsu(char *str);
void scrndump(void);
void windump(int srow, int scol, int erow, int ecol);

#ifdef __cplusplus
}
#endif

#endif /* __CXLPRN_H */
