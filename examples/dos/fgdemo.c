/* A tour of <fastgraf.h>: the best mode of the adapter (or the one given
 * on the command line), shapes, paint, text, images and bitmaps, and the
 * palette.
 *
 *     FGDEMO        the best 16- or 256-color mode
 *     FGDEMO 19     VGA 320x200 in 256 colors
 *
 * Build: clang --target=i8086-pc-msdos -Os fgdemo.c -lgraph -o fgdemo.exe
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <fastgraf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
    int old = fg_getmode();
    int mode = argc > 1 ? atoi(argv[1]) : fg_automode();
    if (!fg_testmode(mode, 1)) {
        printf("Mode %d is not available.\n", mode);
        return 1;
    }
    fg_setmode(mode);
    int mx = fg_getmaxx(), my = fg_getmaxy(), nc = fg_colors();
    int hi = nc - 1 > 15 ? 15 : nc - 1;

    /* A frame and a title. */
    fg_setcolor(hi);
    fg_box(0, mx, 0, my);
    char title[48];
    snprintf(title, sizeof title, "Fastgraph mode %d: %dx%d, %d colors", mode, mx + 1, my + 1, nc);
    fg_locate(1, 2);
    fg_text(title, (int)strlen(title));

    /* Rectangles in each color. */
    int bw = (mx - 20) / 16;
    for (int i = 0; i < 16; i++) {
        fg_setcolor(i % nc);
        fg_rect(10 + i * bw, 8 + (i + 1) * bw - 2, 24, 24 + my / 10);
    }

    /* Filled and outlined circles and ellipses. */
    int cy = my / 3 + 10;
    fg_setcolor(nc > 4 ? 12 : 2);
    fg_move(mx / 6, cy);
    fg_circlef(my / 8);
    fg_setcolor(hi);
    fg_circle(my / 8 + 4);
    fg_setcolor(nc > 4 ? 10 : 1);
    fg_move(mx / 2, cy);
    fg_ellipsef(mx / 8, my / 12);
    fg_setcolor(nc > 4 ? 14 : 3 % nc);
    fg_ellipse(mx / 8 + 6, my / 12 + 4);

    /* A polygon, filled, and a dashed star around it. */
    int px = mx * 5 / 6, s = my / 8;
    int poly[] = { px, cy - s, px + s, cy, px, cy + s, px - s, cy };
    fg_setcolor(nc > 4 ? 9 : 1);
    fg_polyfill(poly, NULL, 4);
    fg_setcolor(hi);
    fg_move(px - s - 6, cy - s - 6);
    fg_dash(px + s + 6, cy + s + 6, 0xf0f0);
    fg_move(px + s + 6, cy - s - 6);
    fg_dash(px - s - 6, cy + s + 6, 0xf0f0);

    /* fg_paint: a box with a thick border, painted inside. */
    int by = my * 2 / 3 - 10;
    fg_setcolor(hi);
    fg_boxdepth(3, 2);
    fg_box(10, mx / 3, by - my / 10, by + my / 10);
    fg_boxdepth(1, 1);
    fg_setcolor(nc > 4 ? 5 : 2 % nc);
    fg_paint(mx / 6, by);
    fg_setcolor(hi);
    fg_move(mx / 6, by + 4);
    fg_justify(0, 0);
    fg_print("fg_paint", 8);
    fg_justify(-1, -1);

    /* A bitmap drawn in several colors, and an image copied with a
     * mirror. */
    static const char arrow[] = {
        0x01, 0x80, 0x03, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc,
        0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0,
    };
    for (int i = 0; i < 6; i++) {
        fg_setcolor((i + 9) % nc ? (i + 9) % nc : 1);
        fg_move(mx / 3 + 20 + i * 20, by + 6);
        fg_drawmap(arrow, 2, 12);
    }
    static char img[40 * 24];
    fg_move(mx / 3 + 20, by + 6);
    fg_getimage(img, 40, 24);
    fg_move(mx / 3 + 20, by + 40);
    fg_revimage(img, 40, 24);

    /* In 256-color modes, a ramp of the palette. */
    if (nc == 256) {
        for (int i = 0; i < 64; i++) {
            fg_setrgb(64 + i, i, 0, 63 - i);
            fg_setcolor(64 + i);
            fg_rect(mx * 2 / 3 + i * (mx / 3 - 10) / 64, mx * 2 / 3 + (i + 1) * (mx / 3 - 10) / 64,
                    by - 20, by + 20);
        }
    } else {
        fg_setcolor(hi);
        fg_move(mx * 2 / 3, by);
        for (int i = 0; i < 10; i++)
            fg_drawrel(i % 2 ? 20 : -10, -8 + i);
    }

    fg_setcolor(hi);
    fg_locate(my / 8 - 2, 2);
    fg_text("Press a key", 11);
    fg_waitkey();
    fg_setmode(old);
    fg_reset();
    return 0;
}
