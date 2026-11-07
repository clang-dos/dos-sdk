/* CXL compatible library: string functions.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <ctype.h>
#include <stdio.h>
#include "cxl-local.h"
#include "cxlstr.h"

static int ieq(int a, int b)
{
    return toupper((unsigned char)a) == toupper((unsigned char)b);
}

/* Finds needle in hay, optionally ignoring case. */
static char *find(char *hay, const char *needle, int icase)
{
    size_t n = strlen(needle);
    if (!n)
        return NULL;
    for (; *hay; hay++) {
        size_t i;
        for (i = 0; i < n && hay[i]; i++)
            if (icase ? !ieq(hay[i], needle[i]) : hay[i] != needle[i])
                break;
        if (i == n)
            return hay;
    }
    return NULL;
}

int strblank(char *str)
{
    for (; *str; str++)
        if (!isspace((unsigned char)*str))
            return 0;
    return 1;
}

char *strltrim(char *str)
{
    char *p = str;
    while (isspace((unsigned char)*p))
        p++;
    if (p != str)
        memmove(str, p, strlen(p) + 1);
    return str;
}

char *strtrim(char *str)
{
    size_t n = strlen(str);
    while (n && isspace((unsigned char)str[n - 1]))
        n--;
    str[n] = 0;
    return str;
}

char *strrtrim(char *str)
{
    return strtrim(str);
}

char *strbtrim(char *str)
{
    return strltrim(strtrim(str));
}

int strchg(char *str, int oldch, int newch)
{
    int n = 0;
    for (; *str; str++)
        if (*str == (char)oldch) {
            *str = newch;
            n++;
        }
    return n;
}

int strichg(char *str, int oldch, int newch)
{
    int n = 0;
    for (; *str; str++)
        if (ieq(*str, oldch)) {
            *str = newch;
            n++;
        }
    return n;
}

unsigned long strchksum(char *str)
{
    unsigned long s = 0;
    while (*str)
        s += (unsigned char)*str++;
    return s;
}

unsigned long strichksum(char *str)
{
    unsigned long s = 0;
    while (*str)
        s += (unsigned char)toupper((unsigned char)*str++);
    return s;
}

/* XOR with the repeating key, leaving a byte alone when the result would
 * be NUL so that the string keeps its length. Running it twice with the
 * same key gives back the original. */
char *strcode(char *str, char *key)
{
    char *p, *k = key;
    if (!key || !*key)
        return str;
    for (p = str; *p; p++) {
        char c = *p ^ *k;
        if (c)
            *p = c;
        if (!*++k)
            k = key;
    }
    return str;
}

static char *del(char *substr, char *str, int icase, int all)
{
    size_t n = strlen(substr);
    char *p, *from = str;
    int found = 0;
    while ((p = find(from, substr, icase)) != NULL) {
        memmove(p, p + n, strlen(p + n) + 1);
        found = 1;
        if (!all)
            break;
        from = p;
    }
    return found ? str : NULL;
}

char *strdel(char *substr, char *str)   { return del(substr, str, 0, 0); }
char *strdela(char *substr, char *str)  { return del(substr, str, 0, 1); }
char *stridel(char *substr, char *str)  { return del(substr, str, 1, 0); }
char *stridela(char *substr, char *str) { return del(substr, str, 1, 1); }

char *strinc(char *str1, char *str2)  { return find(str2, str1, 0); }
char *striinc(char *str1, char *str2) { return find(str2, str1, 1); }

char *strins(char *instr, char *str, int st_pos)
{
    size_t n = strlen(instr), len = strlen(str);
    if (st_pos < 0)
        st_pos = 0;
    if ((size_t)st_pos > len)
        st_pos = len;
    memmove(str + st_pos + n, str + st_pos, len - st_pos + 1);
    memcpy(str + st_pos, instr, n);
    return str;
}

int strocc(char *str, int ch)
{
    int n = 0;
    for (; *str; str++)
        if (*str == (char)ch)
            n++;
    return n;
}

int striocc(char *str, int ch)
{
    int n = 0;
    for (; *str; str++)
        if (ieq(*str, ch))
            n++;
    return n;
}

static int socc(char *str1, char *str2, int icase)
{
    int n = 0;
    size_t len = strlen(str1);
    char *p = str2;
    if (!len)
        return 0;
    while ((p = find(p, str1, icase)) != NULL) {
        n++;
        p += len;
    }
    return n;
}

int strsocc(char *str1, char *str2)  { return socc(str1, str2, 0); }
int strisocc(char *str1, char *str2) { return socc(str1, str2, 1); }

static char *rep(char *str, char *search, char *replace, int icase, int all)
{
    size_t sn = strlen(search), rn = strlen(replace);
    char *p, *from = str;
    int found = 0;
    while ((p = find(from, search, icase)) != NULL) {
        memmove(p + rn, p + sn, strlen(p + sn) + 1);
        memcpy(p, replace, rn);
        found = 1;
        if (!all)
            break;
        from = p + rn;
    }
    return found ? str : NULL;
}

char *strsrep(char *str, char *search, char *replace)  { return rep(str, search, replace, 0, 0); }
char *strisrep(char *str, char *search, char *replace) { return rep(str, search, replace, 1, 0); }
char *strschg(char *str, char *find_, char *replace)   { return rep(str, find_, replace, 0, 1); }
char *strischg(char *str, char *find_, char *replace)  { return rep(str, find_, replace, 1, 1); }

static char *dupn(const char *s, size_t n)
{
    char *d = malloc(n + 1);
    if (d) {
        memcpy(d, s, n);
        d[n] = 0;
    }
    return d;
}

char *strleft(char *str, int num_chars)
{
    size_t len = strlen(str);
    if (num_chars < 0)
        num_chars = 0;
    return dupn(str, (size_t)num_chars < len ? (size_t)num_chars : len);
}

char *strright(char *str, int num_chars)
{
    size_t len = strlen(str);
    if (num_chars < 0)
        num_chars = 0;
    if ((size_t)num_chars > len)
        num_chars = len;
    return dupn(str + len - num_chars, num_chars);
}

char *strmid(char *str, int st_pos, int num_chars)
{
    size_t len = strlen(str);
    if (st_pos < 0)
        st_pos = 0;
    if ((size_t)st_pos > len)
        st_pos = len;
    if (num_chars < 0)
        num_chars = 0;
    if ((size_t)num_chars > len - st_pos)
        num_chars = len - st_pos;
    return dupn(str + st_pos, num_chars);
}

/* Justification keeps the length, moving the spaces to the other side. */
char *strljust(char *str)
{
    size_t len = strlen(str), k = 0;
    while (str[k] == ' ')
        k++;
    if (k) {
        memmove(str, str + k, len - k);
        memset(str + len - k, ' ', k);
    }
    return str;
}

char *strrjust(char *str)
{
    size_t len = strlen(str), k = 0;
    while (k < len && str[len - 1 - k] == ' ')
        k++;
    if (k && k < len) {
        memmove(str + k, str, len - k);
        memset(str, ' ', k);
    }
    return str;
}

char *strrol(char *str, int count)
{
    size_t len = strlen(str);
    if (!len)
        return str;
    count %= (int)len;
    if (count < 0)
        count += len;
    while (count-- > 0) {
        char c = str[0];
        memmove(str, str + 1, len - 1);
        str[len - 1] = c;
    }
    return str;
}

char *strror(char *str, int count)
{
    size_t len = strlen(str);
    if (!len)
        return str;
    count %= (int)len;
    if (count < 0)
        count += len;
    return strrol(str, (int)len - count);
}

char *strshl(char *str, int count)
{
    size_t len = strlen(str);
    if (count <= 0)
        return str;
    if ((size_t)count > len)
        count = len;
    memmove(str, str + count, len - count);
    memset(str + len - count, ' ', count);
    return str;
}

char *strshr(char *str, int count)
{
    size_t len = strlen(str);
    if (count <= 0)
        return str;
    if ((size_t)count > len)
        count = len;
    memmove(str + count, str, len - count);
    memset(str, ' ', count);
    return str;
}

char *strsetsz(char *str, int newsize)
{
    size_t len = strlen(str);
    if (newsize < 0)
        newsize = 0;
    if ((size_t)newsize > len)
        memset(str + len, ' ', newsize - len);
    str[newsize] = 0;
    return str;
}

int touplow(char *str, char *pos, int ch)
{
    int prev = pos > str ? (unsigned char)pos[-1] : 0;
    if (!prev || (!isalnum(prev) && prev != '\''))
        return toupper(ch);
    return tolower(ch);
}

char *struplow(char *str)
{
    char *p;
    for (p = str; *p; p++)
        *p = touplow(str, p, (unsigned char)*p);
    return str;
}

/* Match score: length of the longest common subsequence, ignoring case,
 * with a bonus for each leading character in common. */
int strmatch(char *str1, char *str2)
{
    int n1 = strlen(str1), n2 = strlen(str2), i, j, score, pre = 0;
    int *row;
    if (!n1 || !n2)
        return 0;
    row = calloc(n2 + 1, sizeof(int));
    if (!row)
        return 0;
    for (i = 1; i <= n1; i++) {
        int diag = 0;
        for (j = 1; j <= n2; j++) {
            int up = row[j];
            if (ieq(str1[i - 1], str2[j - 1]))
                row[j] = diag + 1;
            else if (row[j - 1] > row[j])
                row[j] = row[j - 1];
            diag = up;
        }
    }
    score = row[n2];
    free(row);
    while (str1[pre] && str2[pre] && ieq(str1[pre], str2[pre]))
        pre++;
    return score * 2 + pre - (n1 > n2 ? n1 - n2 : n2 - n1);
}

char *strbmatch(char *str, char *strarr[])
{
    char *best = NULL;
    int bs = -32767, i;
    for (i = 0; strarr[i]; i++) {
        int s = strmatch(str, strarr[i]);
        if (s > bs) {
            bs = s;
            best = strarr[i];
        }
    }
    return best;
}

/* CXL field strings: numbers right justified with spaces; a real number
 * is the whole part followed by the fraction, without a decimal point. */
void cvtic(char *field, int value, int size)
{
    char tmp[16];
    int n = snprintf(tmp, sizeof tmp, "%d", value), i;
    if (n > size) {
        for (i = 0; i < size; i++)
            field[i] = '*';
    } else {
        memset(field, ' ', size - n);
        memcpy(field + size - n, tmp, n);
    }
    field[size] = 0;
}

int cvtci(char *field)
{
    return atoi(field);
}

void cvtfc(char *field, double value, int wholesize, int fracsize)
{
    char tmp[48];
    int n, i;
    char *dot;
    n = snprintf(tmp, sizeof tmp, "%.*f", fracsize, value);
    dot = strchr(tmp, '.');
    i = dot ? (int)(dot - tmp) : n;
    if (i > wholesize) {
        memset(field, '*', wholesize + fracsize);
    } else {
        memset(field, ' ', wholesize - i);
        memcpy(field + wholesize - i, tmp, i);
        for (n = 0; n < fracsize; n++)
            field[wholesize + n] = (dot && dot[1 + n]) ? dot[1 + n] : '0';
    }
    field[wholesize + fracsize] = 0;
}

double cvtcf(char *field, int wholesize, int fracsize)
{
    char tmp[48];
    int i, k = 0, len = strlen(field);
    for (i = 0; i < wholesize && i < len && k < 40; i++)
        if (field[i] != ' ')
            tmp[k++] = field[i];
    tmp[k++] = '.';
    for (i = wholesize; i < wholesize + fracsize && i < len && k < 46; i++)
        tmp[k++] = field[i] == ' ' ? '0' : field[i];
    tmp[k] = 0;
    return atof(tmp);
}
