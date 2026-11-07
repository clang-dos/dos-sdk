/* <graphics.h>: full-screen graphics for 16-bit DOS, with the functions of
 * the Borland Graphics Interface (BGI) of Turbo C and Borland C++.
 *
 * The drivers are built in: CGA, MCGA, EGA, VGA and Hercules, and as
 * additions VGA256 (320x200 in 256 colors) and SVGA256 (VESA 256-color
 * modes up to 1024x768). initgraph's path is not used, as nothing is
 * loaded from disk. Text uses the 8x8 font of the video BIOS; every font
 * of settextstyle draws with it, magnified by the character size.
 *
 * Link with -lgraph.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* graphresult() codes. */
enum graphics_errors {
    grOk = 0,
    grNoInitGraph = -1,
    grNotDetected = -2,
    grFileNotFound = -3,
    grInvalidDriver = -4,
    grNoLoadMem = -5,
    grNoScanMem = -6,
    grNoFloodMem = -7,
    grFontNotFound = -8,
    grNoFontMem = -9,
    grInvalidMode = -10,
    grError = -11,
    grIOerror = -12,
    grInvalidFont = -13,
    grInvalidFontNum = -14,
    grInvalidVersion = -18
};

/* Drivers. VGA256 and SVGA256 are additions. */
enum graphics_drivers {
    DETECT = 0,
    CGA,
    MCGA,
    EGA,
    EGA64,
    EGAMONO,
    IBM8514,
    HERCMONO,
    ATT400,
    VGA,
    PC3270,
    VGA256,
    SVGA256,
    CURRENT_DRIVER = -1
};

/* Modes of each driver. */
enum graphics_modes {
    CGAC0 = 0,  /* 320x200, 4 colors: palette 0 */
    CGAC1 = 1,  /* palette 1 */
    CGAC2 = 2,  /* palette 0, bright */
    CGAC3 = 3,  /* palette 1, bright */
    CGAHI = 4,  /* 640x200, 2 colors */
    MCGAC0 = 0,
    MCGAC1 = 1,
    MCGAC2 = 2,
    MCGAC3 = 3,
    MCGAMED = 4, /* 640x200, 2 colors */
    MCGAHI = 5,  /* 640x480, 2 colors */
    EGALO = 0,   /* 640x200, 16 colors, 4 pages */
    EGAHI = 1,   /* 640x350, 16 colors, 2 pages */
    EGA64LO = 0,
    EGA64HI = 1,
    EGAMONOHI = 3,
    HERCMONOHI = 0, /* 720x348, 2 colors, 2 pages */
    VGALO = 0,      /* 640x200, 16 colors, 4 pages */
    VGAMED = 1,     /* 640x350, 16 colors, 2 pages */
    VGAHI = 2,      /* 640x480, 16 colors */
    VGA256LO = 0,   /* 320x200, 256 colors */
    SVGA640x400 = 0,
    SVGA640x480 = 1,
    SVGA800x600 = 2,
    SVGA1024x768 = 3
};

#ifndef __COLORS
#define __COLORS
enum COLORS {
    BLACK,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    BROWN,
    LIGHTGRAY,
    DARKGRAY,
    LIGHTBLUE,
    LIGHTGREEN,
    LIGHTCYAN,
    LIGHTRED,
    LIGHTMAGENTA,
    YELLOW,
    WHITE
};
#endif

/* The EGA and VGA palette values of the sixteen colors, for setpalette. */
enum EGA_COLORS {
    EGA_BLACK = 0,
    EGA_BLUE = 1,
    EGA_GREEN = 2,
    EGA_CYAN = 3,
    EGA_RED = 4,
    EGA_MAGENTA = 5,
    EGA_BROWN = 20,
    EGA_LIGHTGRAY = 7,
    EGA_DARKGRAY = 56,
    EGA_LIGHTBLUE = 57,
    EGA_LIGHTGREEN = 58,
    EGA_LIGHTCYAN = 59,
    EGA_LIGHTRED = 60,
    EGA_LIGHTMAGENTA = 61,
    EGA_YELLOW = 62,
    EGA_WHITE = 63
};

/* The CGA colors of each palette, for drawing in the CGAC modes. */
#define CGA_LIGHTGREEN 1
#define CGA_LIGHTRED 2
#define CGA_YELLOW 3
#define CGA_LIGHTCYAN 1
#define CGA_LIGHTMAGENTA 2
#define CGA_WHITE 3
#define CGA_GREEN 1
#define CGA_RED 2
#define CGA_BROWN 3
#define CGA_CYAN 1
#define CGA_MAGENTA 2
#define CGA_LIGHTGRAY 3

enum line_styles {
    SOLID_LINE = 0,
    DOTTED_LINE = 1,
    CENTER_LINE = 2,
    DASHED_LINE = 3,
    USERBIT_LINE = 4
};

enum line_widths {
    NORM_WIDTH = 1,
    THICK_WIDTH = 3
};

enum fill_patterns {
    EMPTY_FILL,      /* the background color */
    SOLID_FILL,      /* the fill color */
    LINE_FILL,       /* --- */
    LTSLASH_FILL,    /* /// */
    SLASH_FILL,      /* /// thick */
    BKSLASH_FILL,    /* \\\ thick */
    LTBKSLASH_FILL,  /* \\\ */
    HATCH_FILL,      /* +++ */
    XHATCH_FILL,     /* xxx */
    INTERLEAVE_FILL, /* alternate pixels */
    WIDE_DOT_FILL,   /* sparse dots */
    CLOSE_DOT_FILL,  /* dense dots */
    USER_FILL        /* setfillpattern's */
};

/* Writing modes of putimage; setwritemode takes COPY_PUT and XOR_PUT. */
enum putimage_ops {
    COPY_PUT,
    XOR_PUT,
    OR_PUT,
    AND_PUT,
    NOT_PUT
};

enum font_names {
    DEFAULT_FONT = 0,
    TRIPLEX_FONT = 1,
    SMALL_FONT = 2,
    SANS_SERIF_FONT = 3,
    GOTHIC_FONT = 4,
    SCRIPT_FONT = 5,
    SIMPLEX_FONT = 6,
    TRIPLEX_SCR_FONT = 7,
    COMPLEX_FONT = 8,
    EUROPEAN_FONT = 9,
    BOLD_FONT = 10
};

#define HORIZ_DIR 0
#define VERT_DIR 1
#define USER_CHAR_SIZE 0

enum text_just {
    LEFT_TEXT = 0,
    CENTER_TEXT = 1,
    RIGHT_TEXT = 2,
    BOTTOM_TEXT = 0,
    TOP_TEXT = 2
};

#define CLIP_ON 1
#define CLIP_OFF 0

/* bar3d's topflag. */
#define TOPON 1
#define TOPOFF 0

#define MAXCOLORS 15

struct palettetype {
    unsigned char size;
    signed char colors[MAXCOLORS + 1];
};

struct linesettingstype {
    int linestyle;
    unsigned upattern;
    int thickness;
};

struct textsettingstype {
    int font;
    int direction;
    int charsize;
    int horiz;
    int vert;
};

struct fillsettingstype {
    int pattern;
    int color;
};

struct pointtype {
    int x, y;
};

struct viewporttype {
    int left, top, right, bottom;
    int clip;
};

struct arccoordstype {
    int x, y;
    int xstart, ystart, xend, yend;
};

/* Starting and stopping. */
void initgraph(int *__graphdriver, int *__graphmode, const char *__pathtodriver);
void closegraph(void);
void detectgraph(int *__graphdriver, int *__graphmode);
int graphresult(void);
char *grapherrormsg(int __errorcode);
void graphdefaults(void);
void restorecrtmode(void);
void setgraphmode(int __mode);
int getgraphmode(void);
int getmaxmode(void);
void getmoderange(int __graphdriver, int *__lomode, int *__himode);
char *getmodename(int __mode_number);
char *getdrivername(void);
unsigned setgraphbufsize(unsigned __bufsize);

/* Drivers and fonts are built in. These are accepted for programs that
 * register Borland's linked-in drivers and fonts, and do nothing. */
int installuserdriver(const char *__name, int (*__detect)(void));
int installuserfont(const char *__name);
int registerbgidriver(void (*__driver)(void));
int registerbgifont(void (*__font)(void));
void EGAVGA_driver(void);
void CGA_driver(void);
void Herc_driver(void);
void ATT_driver(void);
void PC3270_driver(void);
void IBM8514_driver(void);
void triplex_font(void);
void small_font(void);
void sansserif_font(void);
void gothic_font(void);

/* The screen. */
int getmaxx(void);
int getmaxy(void);
int getmaxcolor(void);
void getaspectratio(int *__xasp, int *__yasp);
void setaspectratio(int __xasp, int __yasp);
void cleardevice(void);
void setactivepage(int __page);
void setvisualpage(int __page);

/* The viewport: drawing coordinates count from its top left corner, and
 * with clipping on nothing is drawn outside it. */
void setviewport(int __left, int __top, int __right, int __bottom, int __clip);
void getviewsettings(struct viewporttype *__viewport);
void clearviewport(void);

/* Colors and palettes. */
void setcolor(int __color);
int getcolor(void);
void setbkcolor(int __color);
int getbkcolor(void);
void setpalette(int __colornum, int __color);
void setallpalette(const struct palettetype *__palette);
void getpalette(struct palettetype *__palette);
struct palettetype *getdefaultpalette(void);
int getpalettesize(void);
void setrgbpalette(int __colornum, int __red, int __green, int __blue);

/* Pixels and the current position. */
void putpixel(int __x, int __y, int __color);
unsigned getpixel(int __x, int __y);
void moveto(int __x, int __y);
void moverel(int __dx, int __dy);
int getx(void);
int gety(void);

/* Lines and outlines. */
void setlinestyle(int __linestyle, unsigned __upattern, int __thickness);
void getlinesettings(struct linesettingstype *__lineinfo);
void setwritemode(int __mode);
void line(int __x1, int __y1, int __x2, int __y2);
void lineto(int __x, int __y);
void linerel(int __dx, int __dy);
void rectangle(int __left, int __top, int __right, int __bottom);
void drawpoly(int __numpoints, const int *__polypoints);
void circle(int __x, int __y, int __radius);
void arc(int __x, int __y, int __stangle, int __endangle, int __radius);
void ellipse(int __x, int __y, int __stangle, int __endangle,
             int __xradius, int __yradius);
void getarccoords(struct arccoordstype *__arccoords);

/* Filled shapes: the fill pattern and color inside, an outline in the
 * drawing color (none for bar). */
void setfillstyle(int __pattern, int __color);
void setfillpattern(const char *__upattern, int __color);
void getfillsettings(struct fillsettingstype *__fillinfo);
void getfillpattern(char *__pattern);
void bar(int __left, int __top, int __right, int __bottom);
void bar3d(int __left, int __top, int __right, int __bottom, int __depth,
           int __topflag);
void fillpoly(int __numpoints, const int *__polypoints);
void fillellipse(int __x, int __y, int __xradius, int __yradius);
void pieslice(int __x, int __y, int __stangle, int __endangle, int __radius);
void sector(int __x, int __y, int __stangle, int __endangle, int __xradius,
            int __yradius);
void floodfill(int __x, int __y, int __border);

/* Text. */
void settextstyle(int __font, int __direction, int __charsize);
void settextjustify(int __horiz, int __vert);
void gettextsettings(struct textsettingstype *__texttypeinfo);
void setusercharsize(int __multx, int __divx, int __multy, int __divy);
void outtext(const char *__textstring);
void outtextxy(int __x, int __y, const char *__textstring);
int textwidth(const char *__textstring);
int textheight(const char *__textstring);

/* Images: imagesize bytes hold the rectangle for getimage and putimage,
 * or 0xFFFF when that is more than 64 KiB. */
unsigned imagesize(int __left, int __top, int __right, int __bottom);
void getimage(int __left, int __top, int __right, int __bottom, void *__bitmap);
void putimage(int __left, int __top, const void *__bitmap, int __op);

#ifdef __cplusplus
}
#endif

#endif /* _GRAPHICS_H_ */
