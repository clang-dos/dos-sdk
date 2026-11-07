/* CXL compatible library: system information and miscellany.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <dos.h>
#include <stdio.h>
#include <unistd.h>
#include "cxl-local.h"
#include "cxlstr.h"

static unsigned equipment(void)
{
    union REGS r;
    int86(0x11, &r, &r);
    return r.x.ax;
}

char *cxlver(void)
{
    return "5.2";
}

char *biosver(void)
{
    static char buf[9];
    int i;
    for (i = 0; i < 8; i++)
        buf[i] = peekb(0xf000, 0xfff5 + i);
    buf[8] = 0;
    return buf;
}

int machid(void)
{
    return (unsigned char)peekb(0xf000, 0xfffe);
}

int clockcal(void)
{
    union REGS r;
    r.h.ah = 2;
    r.x.cflag = 1;
    int86(0x1a, &r, &r);
    return !r.x.cflag;
}

int gameport(void)  { return (equipment() >> 12) & 1; }
int mathchip(void)  { return (equipment() >> 1) & 1; }
int numpar(void)    { return (equipment() >> 14) & 3; }
int numser(void)    { return (equipment() >> 9) & 7; }

int numflop(void)
{
    unsigned e = equipment();
    return (e & 1) ? ((e >> 6) & 3) + 1 : 0;
}

unsigned extmem(void)
{
    union REGS r;
    if (machid() != IBMPCAT && machid() != 0xf8 && machid() != 0xfc)
        return 0;
    r.h.ah = 0x88;
    int86(0x15, &r, &r);
    return r.x.cflag ? 0 : r.x.ax;
}

int emsexist(void);
unsigned emstotal(void);

unsigned expmem(void)
{
    return emsexist() ? emstotal() * 16 : 0;
}

unsigned long timer(void)
{
    return *(volatile unsigned long __far *)MK_FP(0x40, 0x6c);
}

void delay_(unsigned duration)
{
    unsigned long start = timer(), now;
    for (;;) {
        now = timer();
        if (now < start) /* past midnight */
            start = now;
        if (now - start >= duration)
            break;
    }
}

/* pitch is the 8253 timer divisor: the tone is 1193180 / pitch hertz. */
void sound_(unsigned pitch, unsigned duration)
{
    unsigned char b;
    if (pitch < 2)
        pitch = 2;
    outportb(0x43, 0xb6);
    outportb(0x42, pitch & 0xff);
    outportb(0x42, pitch >> 8);
    b = inportb(0x61);
    outportb(0x61, b | 3);
    delay_(duration);
    outportb(0x61, b & ~3);
}

void _stdoutch(int ch)
{
    union REGS r;
    r.h.ah = 2;
    r.h.dl = ch;
    intdos(&r, &r);
}

void beep(void)
{
    _stdoutch(7);
}

int tabstop(int col, int tabwidth)
{
    if (tabwidth <= 0)
        tabwidth = 1;
    return (col / tabwidth + 1) * tabwidth;
}

char *sysdate(int dtype)
{
    static const char *const month[12] = {
        "January", "February", "March", "April", "May", "June", "July",
        "August", "September", "October", "November", "December"
    };
    static char buf[24];
    union REGS r;
    int y, m, d;
    r.h.ah = 0x2a;
    intdos(&r, &r);
    y = r.x.cx;
    m = r.h.dh;
    d = r.h.dl;
    if (m < 1 || m > 12)
        m = 1;
    switch (dtype) {
    case 0: snprintf(buf, sizeof buf, "%s %d, %d", month[m - 1], d, y); break;
    case 1: snprintf(buf, sizeof buf, "%d %.3s %02d", d, month[m - 1], y % 100); break;
    case 2: snprintf(buf, sizeof buf, "%d-%d-%02d", m, d, y % 100); break;
    case 3: snprintf(buf, sizeof buf, "%d/%d/%02d", m, d, y % 100); break;
    case 4: snprintf(buf, sizeof buf, "%d/%d/%02d", d, m, y % 100); break;
    default: snprintf(buf, sizeof buf, "%02d/%02d/%02d", m, d, y % 100); break;
    }
    return buf;
}

char *systime(int ttype)
{
    static char buf[16];
    union REGS r;
    int h, m, s, c, h12;
    r.h.ah = 0x2c;
    intdos(&r, &r);
    h = r.h.ch;
    m = r.h.cl;
    s = r.h.dh;
    c = r.h.dl;
    h12 = h % 12 ? h % 12 : 12;
    switch (ttype) {
    case 0: snprintf(buf, sizeof buf, "%02d:%02d:%02d.%02d", h, m, s, c); break;
    case 1: snprintf(buf, sizeof buf, "%02d:%02d:%02d", h, m, s); break;
    case 2: snprintf(buf, sizeof buf, "%d:%02d %s", h12, m, h < 12 ? "AM" : "PM"); break;
    case 3: snprintf(buf, sizeof buf, "%d:%02d%c", h12, m, h < 12 ? 'a' : 'p'); break;
    case 4: snprintf(buf, sizeof buf, "%d:%02d", h12, m); break;
    default: snprintf(buf, sizeof buf, "%02d:%02d", h12, m); break;
    }
    return buf;
}

char *randfile(void)
{
    static char buf[13];
    static unsigned long seed;
    int tries, i;
    if (!seed)
        seed = timer() * 2654435761UL + 1;
    for (tries = 0; tries < 100; tries++) {
        for (i = 0; i < 8; i++) {
            seed = seed * 1103515245UL + 12345;
            buf[i] = 'A' + (int)((seed >> 16) % 26);
        }
        strcpy(buf + 8, ".$$$");
        if (access(buf, F_OK) != 0)
            break;
    }
    return buf;
}

/* XOR with the repeating key, in place; running it again decrypts. */
int fcrypt(char *file, char *key)
{
    FILE *f;
    char buf[256];
    long pos = 0;
    size_t n, i, k = 0, klen = strlen(key);
    if (!klen)
        return 1;
    f = fopen(file, "r+b");
    if (!f)
        return 1;
    for (;;) {
        n = fread(buf, 1, sizeof buf, f);
        if (!n)
            break;
        for (i = 0; i < n; i++) {
            buf[i] ^= key[k];
            if (++k == klen)
                k = 0;
        }
        if (fseek(f, pos, SEEK_SET) || fwrite(buf, 1, n, f) != n) {
            fclose(f);
            return 1;
        }
        pos += n;
        if (fseek(f, pos, SEEK_SET))
            break;
    }
    return fclose(f) != 0;
}
