/* CXL compatible library: format strings and single-line input.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <ctype.h>
#include "cxl-local.h"
#include "cxlstr.h"

static int isfilech(int ch)
{
    if (isalnum(ch) || ch >= 128)
        return 1;
    return ch && strchr("!#$%&'()-@^_`{}~.:\\", ch) != NULL;
}

int cvaltype(int ch, int ctype)
{
    switch (ctype) {
    case '#': return isdigit(ch) != 0;
    case '%': return isdigit(ch) || ch == ' ';
    case '9': return isdigit(ch) || ch == '.' || ch == '-' || ch == '+';
    case '?': return 1;
    case '*': return ch >= ' ' && ch != 127;
    case 'A': case 'L': case 'M': case 'P': case 'U':
        return isalpha(ch) || ch == ' ';
    case 'D': return isdigit(ch) || ch == '-' || ch == '/';
    case 'F': return isfilech(ch);
    case 'H': return isxdigit(ch) != 0;
    case 'T': return isdigit(ch) || ch == '(' || ch == ')' || ch == '-' || ch == ' ';
    case 'W': return isfilech(ch) || ch == '*' || ch == '?';
    case 'X': return isalnum(ch) || ch == ' ';
    case 'Y': return ch == 'Y' || ch == 'y' || ch == 'N' || ch == 'n';
    }
    return -1;
}

int _cxl_fmtparse(const char *fmt, int toggles, int flags,
                  struct _cxl_slot **out)
{
    struct _cxl_slot *v = NULL;
    int pass, n = 0;
    *out = NULL;

    /* The first pass counts, the second fills in. */
    for (pass = 0; pass < 2; pass++) {
        const char *p = fmt;
        int fl = flags, ad = 0;
        n = 0;
        while (*p) {
            int c = (unsigned char)*p++;
            struct _cxl_slot s;
            s.flags = fl;
            s.attrd = ad;
            s.set = NULL;
            s.setlen = 0;
            s.ch = c;
            if (c == ' ')
                continue;
            if (c == '\'' || c == '"') {
                while (*p && (unsigned char)*p != c) {
                    if (v) {
                        s.kind = SL_LIT;
                        s.ch = *p;
                        v[n] = s;
                    }
                    n++;
                    p++;
                }
                if (!*p)
                    return -1;
                p++;
                continue;
            }
            if (c == '!') {
                if (!toggles)
                    return -1;
                while (*p && *p != '!') {
                    switch (*p) {
                    case '+': ad++; break;
                    case '-': ad--; break;
                    case 'C': fl ^= SF_COPY; break;
                    case 'E': fl ^= SF_ESC; break;
                    case 'L': fl ^= SF_LOWER; break;
                    case 'M': fl ^= SF_MIXED; break;
                    case 'P': fl ^= SF_PASS; break;
                    case 'R': fl ^= SF_RET; break;
                    case 'U': fl ^= SF_UPPER; break;
                    default: return -1;
                    }
                    p++;
                }
                if (!*p)
                    return -1;
                p++;
                continue;
            }
            if (c == '<' || c == '[') {
                int end = (c == '<') ? '>' : ']';
                const char *st = p;
                while (*p && *p != end)
                    p++;
                if (!*p || p == st || p - st > 255)
                    return -1;
                s.kind = (c == '<') ? SL_INCL : SL_EXCL;
                s.set = st;
                s.setlen = (unsigned char)(p - st);
                p++;
            } else if (c == '.') {
                s.kind = toggles ? SL_LIT : SL_DECIMAL;
            } else if (cvaltype('0', c) >= 0) {
                s.kind = SL_INPUT;
            } else {
                return -1;
            }
            if (v)
                v[n] = s;
            n++;
        }
        if (pass == 0) {
            if (n == 0)
                return 0;
            v = malloc(n * sizeof *v);
            if (!v)
                return -1;
        }
    }
    *out = v;
    return n;
}

int _cxl_slotok(const struct _cxl_slot *s, int ch, int prev, int fconv)
{
    int fl = s->flags;
    ch &= 0xff;
    if (ch < ' ' && s->kind != SL_INPUT)
        return -1;
    switch (s->kind) {
    case SL_INPUT:
        if (ch < ' ' && s->ch != '?')
            return -1;
        if (cvaltype(ch, s->ch) != 1)
            return -1;
        if (s->ch == 'L')
            fl |= SF_LOWER;
        else if (s->ch == 'M')
            fl |= SF_MIXED;
        else if (s->ch == 'U')
            fl |= SF_UPPER;
        break;
    case SL_INCL:
        if (!memchr(s->set, ch, s->setlen))
            return -1;
        break;
    case SL_EXCL:
        if (memchr(s->set, ch, s->setlen))
            return -1;
        break;
    default:
        return -1;
    }
    switch (fconv) {
    case 'L': fl |= SF_LOWER; break;
    case 'M': fl |= SF_MIXED; break;
    case 'U': fl |= SF_UPPER; break;
    }
    if (fl & SF_UPPER)
        ch = toupper(ch);
    else if (fl & SF_LOWER)
        ch = tolower(ch);
    else if (fl & SF_MIXED)
        ch = (prev == 0 || prev == ' ') ? toupper(ch) : tolower(ch);
    return ch;
}

static int slot_echo(const struct _cxl_slot *s, int ch)
{
    if ((s->flags & SF_PASS) || (s->kind == SL_INPUT && s->ch == 'P'))
        return ' ';
    return ch;
}

int _cxl_lineinput(const struct _cxl_lineio *io, char *str, const char *fmt,
                   int escflag, int toggles)
{
    struct _cxl_slot *sl;
    unsigned char *val;
    int *pr, *pc;
    int n, cur = 0, ret = 0, i, k;
    int start = SF_RET | (escflag ? SF_ESC : 0);

    n = _cxl_fmtparse(fmt, toggles, start, &sl);
    if (n < 0)
        return 2;
    val = malloc(n + 1);
    pr = malloc((n + 1) * sizeof(int));
    pc = malloc((n + 1) * sizeof(int));
    if (!val || !pr || !pc) {
        free(val);
        free(pr);
        free(pc);
        free(sl);
        return 2;
    }

#define ATTR(i) (io->attr + ((i) < n ? sl[i].attrd : 0))
#define SHOW_LITERALS()                                                     \
    do {                                                                    \
        while (cur < n && sl[cur].kind >= SL_LIT) {                         \
            io->getpos(&pr[cur], &pc[cur]);                                 \
            io->tty(sl[cur].ch, ATTR(cur));                                 \
            cur++;                                                          \
        }                                                                   \
        io->getpos(&pr[cur], &pc[cur]);                                     \
    } while (0)

    SHOW_LITERALS();
    for (;;) {
        unsigned key = getxch();
        int ch = key & 0xff;
        int fl = cur < n ? sl[cur].flags : (n ? sl[n - 1].flags : start);

        if (ch == 0x1b) {
            if (fl & SF_ESC) {
                ret = 1;
                break;
            }
        } else if (ch == '\r') {
            if (cur == n || (fl & SF_RET))
                break;
        } else if (ch == '\b') {
            for (k = cur - 1; k >= 0 && sl[k].kind >= SL_LIT; k--)
                ;
            if (k >= 0) {
                for (i = k; i < cur; i++)
                    if (sl[i].kind < SL_LIT || sl[i].ch >= ' ')
                        io->putat(pr[i], pc[i], ' ', ATTR(i));
                cur = k;
                io->setpos(pr[k], pc[k]);
            }
        } else if (ch && cur < n) {
            int prev = 0, c;
            for (k = cur - 1; k >= 0; k--)
                if (sl[k].kind < SL_LIT) {
                    prev = val[k];
                    break;
                }
            c = _cxl_slotok(&sl[cur], ch, prev, 0);
            if (c >= 0) {
                val[cur] = c;
                io->tty(slot_echo(&sl[cur], c), ATTR(cur));
                cur++;
                SHOW_LITERALS();
            }
        }
    }
#undef ATTR
#undef SHOW_LITERALS

    k = 0;
    if (!ret)
        for (i = 0; i < cur; i++) {
            if (sl[i].kind < SL_LIT)
                str[k++] = val[i];
            else if ((sl[i].flags & SF_COPY) && sl[i].ch >= ' ')
                str[k++] = sl[i].ch;
        }
    str[k] = 0;
    free(val);
    free(pr);
    free(pc);
    free(sl);
    return ret;
}

int _cxl_strinput(const struct _cxl_lineio *io, char *str, int maxchars,
                  int esc)
{
    int n = 0;
    for (;;) {
        unsigned key = getxch();
        int ch = key & 0xff;
        if (ch == 0x1b && esc) {
            str[0] = 0;
            return 1;
        }
        if (ch == '\r')
            break;
        if (ch == '\b') {
            if (n > 0) {
                int r, c;
                n--;
                io->getpos(&r, &c);
                io->tty('\b', io->attr);
                io->getpos(&r, &c);
                io->putat(r, c, ' ', io->attr);
            }
            continue;
        }
        if (ch >= ' ' && (maxchars < 0 || n < maxchars)) {
            str[n++] = ch;
            io->tty(ch, io->attr);
        }
    }
    str[n] = 0;
    return 0;
}
