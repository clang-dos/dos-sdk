/* Text in graphics modes, drawn with the 8x8 font of the video BIOS.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "graph-local.h"
#include <string.h>

/* The magnification across and down. */
static int
scale_x(void)
{
    if (__g.charsize)
        return __g.charsize;
    int s = __g.userdx ? __g.usermx / __g.userdx : 1;
    return s < 1 ? 1 : s;
}

static int
scale_y(void)
{
    if (__g.charsize)
        return __g.charsize;
    int s = __g.userdy ? __g.usermy / __g.userdy : 1;
    return s < 1 ? 1 : s;
}

void
settextstyle(int font, int direction, int charsize)
{
    if (font < DEFAULT_FONT || font > BOLD_FONT) {
        __g.result = grInvalidFontNum;
        return;
    }
    if (charsize < 0 || charsize > 10)
        charsize = 10;
    __g.font = font;
    __g.dir = direction == VERT_DIR ? VERT_DIR : HORIZ_DIR;
    __g.charsize = charsize;
}

void
setusercharsize(int multx, int divx, int multy, int divy)
{
    __g.usermx = multx;
    __g.userdx = divx;
    __g.usermy = multy;
    __g.userdy = divy;
    __g.charsize = USER_CHAR_SIZE;
}

void
settextjustify(int horiz, int vert)
{
    if (horiz < LEFT_TEXT || horiz > RIGHT_TEXT || vert < BOTTOM_TEXT || vert > TOP_TEXT) {
        __g.result = grError;
        return;
    }
    __g.hjust = horiz;
    __g.vjust = vert;
}

void
gettextsettings(struct textsettingstype *ti)
{
    ti->font = __g.font;
    ti->direction = __g.dir;
    ti->charsize = __g.charsize;
    ti->horiz = __g.hjust;
    ti->vert = __g.vjust;
}

int
textwidth(const char *s)
{
    return (int)strlen(s) * 8 * (__g.dir == VERT_DIR ? scale_y() : scale_x());
}

int
textheight(const char *s)
{
    (void)s;
    return 8 * (__g.dir == VERT_DIR ? scale_x() : scale_y());
}

static const uint8_t __far *
glyph(unsigned char c)
{
    if (c < 128)
        return __g.font_lo ? __g.font_lo + c * 8 : 0;
    return __g.font_hi ? __g.font_hi + (c - 128) * 8 : 0;
}

/* Draw character c with its top left corner at (x, y), across, or, for
 * vertical text, turned a quarter left with its bottom left at (x, y). */
static void
draw_char(int x, int y, unsigned char c, int sx, int sy, int vert)
{
    const uint8_t __far *g = glyph(c);
    if (!g)
        return;
    int color = __g.color;
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        for (int col = 0; col < 8;) {
            if (!(bits & (0x80 >> col))) {
                col++;
                continue;
            }
            int end = col;
            while (end + 1 < 8 && (bits & (0x80 >> (end + 1))))
                end++;
            if (!vert) {
                for (int k = 0; k < sy; k++)
                    __g_span(x + col * sx, x + (end + 1) * sx - 1, y + r * sy + k, color);
            } else {
                /* Row r goes across from the left, column col up. */
                for (int k = col * sx; k < (end + 1) * sx; k++)
                    __g_span(x + r * sy, x + (r + 1) * sy - 1, y - k, color);
            }
            col = end + 1;
        }
    }
}

void
outtextxy(int x, int y, const char *s)
{
    uint8_t op = __g.op;
    __g.op = OP_COPY;
    int sx = scale_x(), sy = scale_y();
    int n = (int)strlen(s);
    if (__g.dir == HORIZ_DIR) {
        int w = n * 8 * sx, h = 8 * sy;
        if (__g.hjust == CENTER_TEXT)
            x -= w / 2;
        else if (__g.hjust == RIGHT_TEXT)
            x -= w;
        if (__g.vjust == CENTER_TEXT)
            y -= h / 2;
        else if (__g.vjust == BOTTOM_TEXT)
            y -= h - 1;
        for (int i = 0; i < n; i++)
            draw_char(x + i * 8 * sx, y, (unsigned char)s[i], sx, sy, 0);
    } else {
        /* Up the screen: a character is 8 * sx high and 8 * sy wide. */
        int len = n * 8 * sx, h = 8 * sy;
        if (__g.hjust == CENTER_TEXT)
            x -= h / 2;
        else if (__g.hjust == RIGHT_TEXT)
            x -= h;
        int bottom = y;
        if (__g.vjust == TOP_TEXT)
            bottom = y + len - 1;
        else if (__g.vjust == CENTER_TEXT)
            bottom = y + len / 2;
        for (int i = 0; i < n; i++)
            draw_char(x, bottom - i * 8 * sx, (unsigned char)s[i], sx, sy, 1);
    }
    __g.op = op;
}

void
outtext(const char *s)
{
    outtextxy(__g.cpx, __g.cpy, s);
    if (__g.dir == HORIZ_DIR && __g.hjust == LEFT_TEXT)
        __g.cpx += textwidth(s);
}
