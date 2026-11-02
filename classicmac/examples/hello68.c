/* Toolbox "hello" for 68k Macs (System 7 and Mac OS 8), declared the way
   classic Mac headers do: "pascal" for the calling convention and the
   trap word as inline code. */
typedef unsigned char Boolean;
typedef const unsigned char *ConstStr255Param;
typedef struct { short top, left, bottom, right; } Rect;
typedef void *WindowPtr;
#define ONEWORDINLINE(w) __attribute__((raw_inline(w)))

#pragma options align=mac68k
typedef struct {
  char privates[76]; long randSeed;
  struct { char *baseAddr; short rowBytes; Rect bounds; } screenBits;
  struct { unsigned short data[16], mask[16]; short v, h; } arrow;
  unsigned char dkGray[8], ltGray[8], gray[8], black[8], white[8];
  void *thePort;
} QDGlobals;
#pragma options align=reset
_Static_assert(sizeof(QDGlobals) == 206, "QDGlobals");

pascal void InitGraf(void *globalPtr) ONEWORDINLINE(0xA86E);
pascal void InitFonts(void) ONEWORDINLINE(0xA8FE);
pascal void InitWindows(void) ONEWORDINLINE(0xA912);
pascal void InitMenus(void) ONEWORDINLINE(0xA930);
pascal void TEInit(void) ONEWORDINLINE(0xA9CC);
pascal void InitDialogs(void *resumeProc) ONEWORDINLINE(0xA97B);
pascal void InitCursor(void) ONEWORDINLINE(0xA850);
pascal WindowPtr NewWindow(void *storage, const Rect *boundsRect, ConstStr255Param title,
                           Boolean visible, short procID, WindowPtr behind,
                           Boolean goAwayFlag, long refCon) ONEWORDINLINE(0xA913);
pascal void SetPort(void *port) ONEWORDINLINE(0xA873);
pascal void MoveTo(short h, short v) ONEWORDINLINE(0xA893);
pascal void DrawString(ConstStr255Param s) ONEWORDINLINE(0xA884);
pascal Boolean Button(void) ONEWORDINLINE(0xA974);

/* FlushEvents is a register-based OS trap: D0 = stopMask << 16 | eventMask. */
static inline void FlushEvents(short what, short stop) {
  register long d0 __asm__("d0") = ((long)stop << 16) | (unsigned short)what;
  __asm__ volatile(".short 0xA032" : "+d"(d0) :: "memory", "d1", "d2", "a0", "a1");
}

QDGlobals qd;
static const char greeting[] = "Hello from clang-dos";

int main(void) {
  InitGraf(&qd.thePort);
  InitFonts();
  FlushEvents(-1, 0);
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(0);
  InitCursor();
  Rect r = {80, 60, 240, 460};
  WindowPtr w = NewWindow(0, &r, "\pclang-dos 68k", 1, 0, (WindowPtr)-1, 0, 0);
  SetPort(w);
  MoveTo(20, 40);
  DrawString("\pHello, 68k Mac! Built with clang.");
  unsigned char buf[64];
  int n = 0;
  for (const char *p = greeting; *p; ++p) buf[++n] = *p;
  buf[0] = n;
  MoveTo(20, 70);
  DrawString(buf);
  while (!Button()) {}
  return 0;
}
