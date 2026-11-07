/* CXL compatible library: LIM expanded memory (INT 67h).
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <dos.h>
#include <stdio.h>
#include "cxl-local.h"
#include "cxlems.h"

static void ems(union REGS *r)
{
    int86(0x67, r, r);
}

int emsexist(void)
{
    static const char name[] = "EMMXXXX0";
    void(__interrupt __far * v)() = _dos_getvect(0x67);
    const char __far *p;
    int i;
    union REGS r;
    if (!v)
        return 0;
    p = (const char __far *)MK_FP(FP_SEG(v), 10);
    for (i = 0; i < 8; i++)
        if (p[i] != name[i])
            return 0;
    r.h.ah = 0x40; /* status */
    ems(&r);
    return r.h.ah == 0;
}

unsigned emsframe(void)
{
    union REGS r;
    r.h.ah = 0x41;
    ems(&r);
    return r.h.ah ? 0 : r.x.bx;
}

unsigned emsfree(void)
{
    union REGS r;
    r.h.ah = 0x42;
    ems(&r);
    return r.h.ah ? 0 : r.x.bx;
}

unsigned emstotal(void)
{
    union REGS r;
    r.h.ah = 0x42;
    ems(&r);
    return r.h.ah ? 0 : r.x.dx;
}

int emsalloc(int numpages)
{
    union REGS r;
    r.h.ah = 0x43;
    r.x.bx = numpages;
    ems(&r);
    return r.h.ah ? -1 : (int)r.x.dx;
}

int emsdealloc(int handle)
{
    union REGS r;
    r.h.ah = 0x45;
    r.x.dx = handle;
    ems(&r);
    return r.h.ah != 0;
}

int emsmap(int handle, int lpage, int ppage)
{
    union REGS r;
    r.h.ah = 0x44;
    r.h.al = ppage;
    r.x.bx = lpage;
    r.x.dx = handle;
    ems(&r);
    return r.h.ah != 0;
}

char *emsver(void)
{
    static char buf[8];
    union REGS r;
    r.h.ah = 0x46;
    ems(&r);
    if (r.h.ah)
        return NULL;
    snprintf(buf, sizeof buf, "%d.%d", r.h.al >> 4, r.h.al & 15);
    return buf;
}

int emsread(char *dest, unsigned emsofs, unsigned numbytes)
{
    unsigned seg = emsframe();
    const char __far *src;
    if (!seg || (unsigned long)emsofs + numbytes > 0x10000UL)
        return 1;
    src = (const char __far *)MK_FP(seg, emsofs);
    while (numbytes--)
        *dest++ = *src++;
    return 0;
}

int emswrite(char *src, unsigned emsofs, unsigned numbytes)
{
    unsigned seg = emsframe();
    char __far *dst;
    if (!seg || (unsigned long)emsofs + numbytes > 0x10000UL)
        return 1;
    dst = (char __far *)MK_FP(seg, emsofs);
    while (numbytes--)
        *dst++ = *src++;
    return 0;
}
