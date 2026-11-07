/* CXL compatible library: boxes and screen files.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdio.h>
#include "cxl-local.h"

/* Code page 437 box characters: corners top-left, top-right, bottom-left,
 * bottom-right, then top, bottom and side lines. */
const unsigned char _cxl_boxch[6][7] = {
    {0xda, 0xbf, 0xc0, 0xd9, 0xc4, 0xc4, 0xb3}, /* single */
    {0xc9, 0xbb, 0xc8, 0xbc, 0xcd, 0xcd, 0xba}, /* double */
    {0xd6, 0xb7, 0xd3, 0xbd, 0xc4, 0xc4, 0xba}, /* single horz, double vert */
    {0xd5, 0xb8, 0xd4, 0xbe, 0xcd, 0xcd, 0xb3}, /* double horz, single vert */
    {0xdb, 0xdb, 0xdb, 0xdb, 0xdf, 0xdc, 0xdb}, /* thick */
    {' ', ' ', ' ', ' ', ' ', ' ', ' '},        /* blank */
};

void _cxl_box(int srow, int scol, int erow, int ecol, int btype, int attr)
{
    const unsigned char *b;
    int i;
    if (btype < 0 || btype > 5)
        btype = 0;
    b = _cxl_boxch[btype];
    for (i = scol + 1; i < ecol; i++) {
        _cxl_vput(srow, i, CELL(b[4], attr));
        _cxl_vput(erow, i, CELL(b[5], attr));
    }
    for (i = srow + 1; i < erow; i++) {
        _cxl_vput(i, scol, CELL(b[6], attr));
        _cxl_vput(i, ecol, CELL(b[6], attr));
    }
    _cxl_vput(srow, scol, CELL(b[0], attr));
    _cxl_vput(srow, ecol, CELL(b[1], attr));
    _cxl_vput(erow, scol, CELL(b[2], attr));
    _cxl_vput(erow, ecol, CELL(b[3], attr));
}

void box_(int srow, int scol, int erow, int ecol, int btype, int attr)
{
    _cxl_box(srow, scol, erow, ecol, btype, attr);
}

/* Screen files hold four 16-bit numbers (start row, start column, end row,
 * end column) and then the character/attribute cells, row by row. */
int wintodisk(int srow, int scol, int erow, int ecol, char *fname)
{
    FILE *f;
    int *buf;
    size_t n;
    int i, err = 0;

    buf = _cxl_save(srow, scol, erow, ecol);
    if (!buf)
        return 1;
    n = (size_t)(buf[2] - buf[0] + 1) * (buf[3] - buf[1] + 1);
    f = fopen(fname, "wb");
    if (!f) {
        free(buf);
        return 1;
    }
    for (i = 0; i < 4; i++) {
        putc(buf[i] & 0xff, f);
        putc((buf[i] >> 8) & 0xff, f);
    }
    for (i = 0; (size_t)i < n; i++) {
        unsigned c = (unsigned)buf[4 + i];
        putc(c & 0xff, f);
        putc(c >> 8, f);
    }
    if (ferror(f))
        err = 1;
    if (fclose(f))
        err = 1;
    free(buf);
    return err;
}

int scrntodisk(char *fname)
{
    return wintodisk(0, 0, _vinfo.numrows - 1, _vinfo.numcols - 1, fname);
}

int disktowin(char *fname)
{
    FILE *f;
    int hdr[4], *buf;
    size_t n, i;
    int k;

    f = fopen(fname, "rb");
    if (!f)
        return 1;
    for (k = 0; k < 4; k++) {
        int lo = getc(f), hi = getc(f);
        if (hi == EOF) {
            fclose(f);
            return 1;
        }
        hdr[k] = (short)(lo | (hi << 8));
    }
    if (hdr[2] < hdr[0] || hdr[3] < hdr[1] || hdr[0] < 0 || hdr[1] < 0) {
        fclose(f);
        return 1;
    }
    n = (size_t)(hdr[2] - hdr[0] + 1) * (hdr[3] - hdr[1] + 1);
    buf = malloc((4 + n) * sizeof(int));
    if (!buf) {
        fclose(f);
        return 1;
    }
    for (k = 0; k < 4; k++)
        buf[k] = hdr[k];
    for (i = 0; i < n; i++) {
        int lo = getc(f), hi = getc(f);
        if (hi == EOF) {
            free(buf);
            fclose(f);
            return 1;
        }
        buf[4 + i] = lo | (hi << 8);
    }
    fclose(f);
    _cxl_restore(buf);
    free(buf);
    return 0;
}

int disktoscrn(char *fname)
{
    return disktowin(fname);
}
