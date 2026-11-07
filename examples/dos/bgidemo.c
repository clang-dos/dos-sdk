/* A tour of <graphics.h>: shapes, fills, lines, text and images on one
 * screen. Run it with a driver and mode to try another adapter:
 *
 *     BGIDEMO            the best mode of the adapter found
 *     BGIDEMO 1 0        CGA, 320x200 in 4 colors (see graphics.h)
 *     BGIDEMO 11 0       VGA256, 320x200 in 256 colors
 *     BGIDEMO 12 1       SVGA256, 640x480 in 256 colors
 *
 * Build: clang --target=i8086-pc-msdos -Os bgidemo.c -lgraph -o bgidemo.exe
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <conio.h>
#include <graphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int maxx, maxy, maxc;

static int
col(int c)
{
    /* In 2- and 4-color modes, spread the 16 colors over what there is. */
    return maxc >= 15 ? c : (c % maxc) + 1;
}

static void
frame(const char *title)
{
    setcolor(col(WHITE));
    rectangle(0, 0, maxx, maxy);
    settextjustify(CENTER_TEXT, TOP_TEXT);
    outtextxy(maxx / 2, 4, title);
    settextjustify(LEFT_TEXT, TOP_TEXT);
    line(0, 14, maxx, 14);
}

int
main(int argc, char **argv)
{
    int gd = DETECT, gm = 0;
    if (argc > 2) {
        gd = atoi(argv[1]);
        gm = atoi(argv[2]);
    }
    initgraph(&gd, &gm, "");
    int err = graphresult();
    if (err != grOk) {
        printf("initgraph: %s\n", grapherrormsg(err));
        return 1;
    }
    maxx = getmaxx();
    maxy = getmaxy();
    maxc = getmaxcolor();

    char title[80];
    snprintf(title, sizeof title, "%s %s, %d colors", getdrivername(),
             getmodename(gm), maxc + 1);
    frame(title);

    /* Units of the layout: the screen in 8 columns and 4 rows. */
    int cw = maxx / 8, rh = (maxy - 16) / 4, top = 16;

    /* Row 1: fill patterns. */
    for (int i = 0; i < 12; i++) {
        int x = 4 + (i % 6) * (cw * 4 / 6), y = top + 2 + (i / 6) * (rh / 2);
        setfillstyle(i, col(i + 1));
        setcolor(col(WHITE));
        bar3d(x, y, x + cw * 4 / 6 - 10, y + rh / 2 - 6, 0, TOPON);
    }

    /* Row 1 right: concentric circles and an ellipse. */
    int cx = cw * 5 + cw / 2, cy = top + rh / 2;
    for (int r = 4; r < rh / 2; r += 4) {
        setcolor(col(1 + (r / 4) % 15));
        circle(cx, cy, r);
    }
    setcolor(col(YELLOW));
    ellipse(cw * 7, cy, 0, 360, cw / 2 + 4, rh / 3);
    setfillstyle(SOLID_FILL, col(LIGHTRED));
    fillellipse(cw * 7, cy, cw / 4, rh / 6);

    /* Row 2: a pie chart and a 3D bar chart. */
    int py = top + rh + rh / 2;
    static const int slices[] = { 0, 70, 150, 220, 290, 360 };
    for (int i = 0; i < 5; i++) {
        setfillstyle(i == 2 ? HATCH_FILL : SOLID_FILL, col(i + 9));
        setcolor(col(WHITE));
        pieslice(cw, py, slices[i], slices[i + 1], rh / 2 - 4);
    }
    for (int i = 0; i < 6; i++) {
        int h = (rh - 20) * (i + 2) / 8;
        setfillstyle(i % 2 ? SOLID_FILL : CLOSE_DOT_FILL, col(i + 1));
        setcolor(col(WHITE));
        bar3d(cw * 2 + 10 + i * (cw / 2), top + 2 * rh - 6 - h,
              cw * 2 + 10 + i * (cw / 2) + cw / 3, top + 2 * rh - 6, cw / 10, TOPON);
    }

    /* Row 2 right: line styles and widths, and a star by fillpoly. */
    for (int s = 0; s < 4; s++) {
        setlinestyle(s, 0, NORM_WIDTH);
        setcolor(col(LIGHTCYAN));
        line(cw * 5, top + rh + 8 + s * 8, cw * 6 - 4, top + rh + 8 + s * 8);
        setlinestyle(s, 0, THICK_WIDTH);
        line(cw * 5, top + rh + 44 + s * 8, cw * 6 - 4, top + rh + 44 + s * 8);
    }
    setlinestyle(SOLID_LINE, 0, NORM_WIDTH);
    int star[22];
    int sx = cw * 7, sy = top + rh + rh / 2, r1 = rh / 2 - 4, r2 = r1 * 2 / 5;
    static const int ang[10][2] = {
        { 0, -100 }, { 22, -31 }, { 95, -31 }, { 36, 12 }, { 59, 81 },
        { 0, 38 }, { -59, 81 }, { -36, 12 }, { -95, -31 }, { -22, -31 },
    };
    for (int i = 0; i < 10; i++) {
        int r = i % 2 ? r2 : r1;
        (void)r;
        star[2 * i] = sx + ang[i][0] * r1 / 100;
        star[2 * i + 1] = sy + ang[i][1] * r1 / 100 * 3 / 4;
    }
    star[20] = star[0];
    star[21] = star[1];
    setcolor(col(YELLOW));
    setfillstyle(SOLID_FILL, col(BROWN));
    fillpoly(11, star);

    /* Row 3: text in sizes and directions. */
    int ty = top + 2 * rh + 6;
    setcolor(col(LIGHTGREEN));
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
    outtextxy(8, ty, "Size 1: The quick brown fox");
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 2);
    setcolor(col(LIGHTMAGENTA));
    outtextxy(8, ty + 12, "Size 2");
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 3);
    setcolor(col(LIGHTCYAN));
    outtextxy(8, ty + 32, "Size 3");
    settextstyle(DEFAULT_FONT, VERT_DIR, 1);
    setcolor(col(YELLOW));
    outtextxy(cw * 3, ty + rh - 10, "Vertical");
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);

    /* Row 3 right: a flood-filled shape with a hole. */
    setcolor(col(WHITE));
    rectangle(cw * 4, ty, cw * 6, ty + rh - 10);
    circle(cw * 5, ty + (rh - 10) / 2, (rh - 10) / 4);
    setfillstyle(XHATCH_FILL, col(LIGHTBLUE));
    floodfill(cw * 4 + 4, ty + 4, col(WHITE));

    /* Row 4: an image, copied with each operation. */
    int iy = top + 3 * rh + 4, iw = cw - 8, ih = rh - 12;
    setfillstyle(SOLID_FILL, col(RED));
    bar(8, iy, 8 + iw, iy + ih);
    setcolor(col(YELLOW));
    line(8, iy, 8 + iw, iy + ih);
    line(8, iy + ih, 8 + iw, iy);
    circle(8 + iw / 2, iy + ih / 2, ih / 3);
    unsigned size = imagesize(8, iy, 8 + iw, iy + ih);
    void *img = size != 0xffff ? malloc(size) : NULL;
    if (img) {
        getimage(8, iy, 8 + iw, iy + ih, img);
        static const char *const ops[] = { "COPY", "XOR", "OR", "AND", "NOT" };
        for (int op = 0; op < 5; op++) {
            int x = 8 + (op + 1) * cw;
            setfillstyle(INTERLEAVE_FILL, col(GREEN));
            bar(x, iy, x + iw, iy + ih);
            putimage(x, iy, img, op);
            setcolor(col(WHITE));
            outtextxy(x, iy + ih + 2, ops[op]);
        }
        free(img);
    }
    /* The last column: palette bars, or the 256 colors. */
    if (maxc == 255) {
        for (int i = 0; i < 256; i++) {
            setcolor(i);
            line(cw * 7 + (i % 16) * (cw / 16 - 0) / 1 * 0 + (i % 16) * (cw - 8) / 16,
                 iy + (i / 16) * ih / 16,
                 cw * 7 + (i % 16) * (cw - 8) / 16 + (cw - 8) / 16 - 1,
                 iy + (i / 16) * ih / 16);
        }
    } else {
        for (int i = 0; i <= maxc && i < 16; i++) {
            setfillstyle(SOLID_FILL, i);
            bar(cw * 7 + i * (cw - 8) / 16, iy, cw * 7 + (i + 1) * (cw - 8) / 16 - 1, iy + ih);
        }
    }

    getch();
    closegraph();
    printf("%s mode %d: %dx%d, %d colors\n", getdrivername(), gm, maxx + 1, maxy + 1, maxc + 1);
    return 0;
}
