/* <fastgraf.h>: the fg_ graphics functions of the Fastgraph library,
 * reimplemented for 16-bit DOS on the drivers of <graphics.h>.
 *
 * Modes are numbered as in Fastgraph (the BIOS mode numbers, plus 11 for
 * Hercules and 24 to 27 for the VESA 256-color modes):
 *
 *     0-3, 7  text          13  320x200, 16 colors (EGA)
 *     4, 5    320x200, 4    14  640x200, 16 colors
 *     6       640x200, 2    16  640x350, 16 colors
 *     11      720x348, 2    17  640x480, 2 colors
 *                           18  640x480, 16 colors
 *     19      320x200, 256  24  640x400, 256 (VESA)
 *     25      640x480, 256  26  800x600, 256
 *     27      1024x768, 256
 *
 * The unchained 256-color modes (20-23), the 16-color SVGA modes (28,
 * 29), and Fastgraph's virtual buffers, sprites, image files and
 * sound are not here.
 *
 * Coordinates are screen pixels, (0, 0) at the top left; the clipping
 * region (fg_setclip) limits every drawing function. Images (fg_getimage,
 * fg_putimage, fg_drwimage) hold a byte per pixel in every mode; bitmaps
 * (fg_drawmap, fg_getmap) a bit per pixel, a set bit drawn in the current
 * color. Image and bitmap positions are their lower left corner, at the
 * graphics cursor.
 *
 * Link with -lgraph.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef _FASTGRAF_H_
#define _FASTGRAF_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Modes. */
int fg_setmode(int mode);   /* returns nothing useful; check fg_getmode */
int fg_getmode(void);
int fg_testmode(int mode, int pages);
int fg_bestmode(int horizontal, int vertical, int pages);
int fg_automode(void);
void fg_reset(void);
int fg_getmaxx(void);
int fg_getmaxy(void);
int fg_colors(void);
int fg_svgainit(int method);
int fg_svgastat(void);
void fg_version(int *major, int *minor);
void fg_initpm(void);

/* Colors and palettes. */
void fg_setcolor(int color);
int fg_getcolor(void);
void fg_palette(int number, int color);
void fg_setrgb(int number, int red, int green, int blue);
void fg_getrgb(int number, int *red, int *green, int *blue);
void fg_setdacs(int start, int count, const char *values);
void fg_getdacs(int start, int count, char *values);
void fg_defcolor(int index, int value);
int fg_getindex(int index);

/* The drawing operation: 0 replace, 1 AND, 2 OR, 3 XOR. */
void fg_setfunc(int mode);

/* Pages. */
void fg_setpage(int page);
int fg_getpage(void);
void fg_setvpage(int page);
int fg_getvpage(void);
void fg_copypage(int source, int dest);
void fg_transfer(int minx, int maxx, int miny, int maxy, int newx, int newy,
                 int source, int dest);

/* Clipping. */
void fg_setclip(int minx, int maxx, int miny, int maxy);

/* Points, lines and shapes. */
void fg_erase(void);
void fg_fillpage(void);
void fg_point(int x, int y);
void fg_pointx(int x, int y);
int fg_getpixel(int x, int y);
void fg_move(int x, int y);
void fg_moverel(int dx, int dy);
int fg_getxpos(void);
int fg_getypos(void);
void fg_draw(int x, int y);
void fg_drawrel(int dx, int dy);
void fg_drawx(int x, int y);
void fg_dash(int x, int y, int pattern);
void fg_dashrel(int dx, int dy, int pattern);
void fg_rect(int minx, int maxx, int miny, int maxy);
void fg_box(int minx, int maxx, int miny, int maxy);
void fg_boxx(int minx, int maxx, int miny, int maxy);
void fg_boxdepth(int xdepth, int ydepth);
void fg_circle(int radius);
void fg_circlef(int radius);
void fg_ellipse(int horiz, int vert);
void fg_ellipsef(int horiz, int vert);
void fg_polygon(const int *xvertices, const int *yvertices, int count);
void fg_polyline(const int *vertices, int count);
void fg_polyfill(const int *vertices, int *work, int count);
void fg_paint(int x, int y);
void fg_flood(int x, int y);

/* Text: fg_text writes at the text cursor (fg_locate, in 8x8 cells),
 * fg_print at the graphics cursor, whose point fg_justify chooses. */
void fg_locate(int row, int column);
void fg_where(int *row, int *column);
void fg_text(const char *string, int n);
void fg_textc(const char *string, int n);
void fg_print(const char *string, int n);
void fg_justify(int xjust, int yjust);
int fg_fontsize(int size);

/* Images and bitmaps. */
void fg_getimage(char *map, int width, int height);
void fg_putimage(const char *map, int width, int height);
void fg_drwimage(const char *map, int width, int height);
void fg_clpimage(const char *map, int width, int height);
void fg_revimage(const char *map, int width, int height);
void fg_flpimage(const char *map, int width, int height);
void fg_getmap(char *map, int width, int height);
void fg_drawmap(const char *map, int width, int height);
int fg_imagesiz(int width, int height);

/* Keyboard. */
void fg_getkey(unsigned char *key, unsigned char *aux);
void fg_intkey(unsigned char *key, unsigned char *aux);
void fg_waitkey(void);
int fg_kbtest(int scan_code);

/* Mouse. */
int fg_mouseini(void);
void fg_mousevis(int state);
void fg_mousepos(int *x, int *y, int *buttons);
void fg_mousemov(int x, int y);
void fg_mouselim(int minx, int maxx, int miny, int maxy);
void fg_mousebut(int number, int *count, int *lastx, int *lasty);

/* Time and sound. */
void fg_waitfor(int ticks);
long fg_getclock(void);
void fg_stall(int delay);
int fg_measure(void);
void fg_sound(int frequency, int duration);
void fg_quiet(void);
void fg_beep(void);

#ifdef __cplusplus
}
#endif

#endif /* _FASTGRAF_H_ */
