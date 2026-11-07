/* CXL compatible library: string functions.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __CXLSTR_H
#define __CXLSTR_H

#ifdef __cplusplus
extern "C" {
#endif

int           cvaltype(int ch, int ctype);
double        cvtcf(char *field, int wholesize, int fracsize);
int           cvtci(char *field);
void          cvtfc(char *field, double value, int wholesize, int fracsize);
void          cvtic(char *field, int value, int size);
int           strblank(char *str);
char         *strbmatch(char *str, char *strarr[]);
char         *strbtrim(char *str);
int           strchg(char *str, int oldch, int newch);
unsigned long strchksum(char *str);
char         *strcode(char *str, char *key);
char         *strdel(char *substr, char *str);
char         *strdela(char *substr, char *str);
int           strichg(char *str, int oldch, int newch);
unsigned long strichksum(char *str);
char         *stridel(char *substr, char *str);
char         *stridela(char *substr, char *str);
char         *striinc(char *str1, char *str2);
char         *strinc(char *str1, char *str2);
char         *strins(char *instr, char *str, int st_pos);
int           striocc(char *str, int ch);
char         *strischg(char *str, char *find, char *replace);
int           strisocc(char *str1, char *str2);
char         *strisrep(char *str, char *search, char *replace);
char         *strleft(char *str, int num_chars);
char         *strljust(char *str);
char         *strltrim(char *str);
int           strmatch(char *str1, char *str2);
char         *strmid(char *str, int st_pos, int num_chars);
int           strocc(char *str, int ch);
char         *strright(char *str, int num_chars);
char         *strrjust(char *str);
char         *strrol(char *str, int count);
char         *strror(char *str, int count);
char         *strrtrim(char *str);
char         *strschg(char *str, char *find, char *replace);
char         *strsetsz(char *str, int newsize);
char         *strshl(char *str, int count);
char         *strshr(char *str, int count);
int           strsocc(char *str1, char *str2);
char         *strsrep(char *str, char *search, char *replace);
char         *strtrim(char *str);
char         *struplow(char *str);
int           touplow(char *str, char *pos, int ch);

#ifdef __cplusplus
}
#endif

#endif /* __CXLSTR_H */
