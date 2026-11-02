typedef unsigned char Str255[256];
typedef const unsigned char *ConstStr255Param;
typedef char *Ptr;
typedef struct { short top, left, bottom, right; } Rect;
#pragma options align=mac68k
typedef struct { Ptr baseAddr; short rowBytes; Rect bounds; } BitMap;
typedef struct { unsigned char pat[8]; } Pattern;
typedef struct { unsigned short data[16], mask[16]; struct { short v, h; } hotSpot; } Cursor;
typedef struct QDGlobals {
  char privates[76]; long randSeed; BitMap screenBits; Cursor arrow;
  Pattern dkGray, ltGray, gray, black, white; void *thePort;
} QDGlobals;
#pragma options align=reset
typedef void *WindowPtr;
_Static_assert(sizeof(QDGlobals) == 206, "QDGlobals");

extern pascal void InitGraf(void *globalPtr);
extern pascal void InitFonts(void);
extern pascal void InitWindows(void);
extern pascal void InitMenus(void);
extern pascal void TEInit(void);
extern pascal void InitDialogs(void *resumeProc);
extern pascal void InitCursor(void);
extern pascal WindowPtr NewCWindow(void *storage, const Rect *r, ConstStr255Param title,
                                   unsigned char visible, short procID, WindowPtr behind,
                                   unsigned char goAway, long refCon);
extern pascal void SetPort(void *port);
extern pascal void MoveTo(short h, short v);
extern pascal void DrawString(ConstStr255Param s);
extern pascal unsigned char Button(void);
extern pascal void ExitToShell(void);

QDGlobals qd;

static const char *greeting = "Hello from clang-dos";

int main(void) {
  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(0);
  InitCursor();
  Rect r = {80, 60, 240, 460};
  WindowPtr w = NewCWindow(0, &r, "\pclang-dos", 1, 0, (WindowPtr)-1, 0, 0);
  SetPort(w);
  MoveTo(20, 40);
  DrawString("\pHello, Mac OS 9! Built with clang.");
  unsigned char buf[64];
  int n = 0;
  for (const char *p = greeting; *p; ++p) buf[++n] = *p;
  buf[0] = n;
  MoveTo(20, 70);
  DrawString(buf);
  while (!Button()) {}
  return 0;
}
