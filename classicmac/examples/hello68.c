/* Toolbox "hello" for 68k Macs (System 7 and 8). Each Toolbox call is an
   A-line trap with Pascal calling conventions: the caller reserves room
   for the result, pushes the arguments left to right, and the trap pops
   them. Until clang knows 'pascal' and trap numbers on 68k, small inline
   asm wrappers do the same. Traps may change d0-d2 and a0-a1.
   Pointers use "r": the M68k inline asm printer drops the '#' from a
   symbol address given as an "i" operand, turning it into a load. */
typedef struct { short top, left, bottom, right; } Rect;
typedef void *WindowPtr;

#define CLOBBERS "memory", "d0", "d1", "d2", "a0", "a1"
#define TRAP0(name, num) \
  static inline void name(void) { __asm__ volatile(".short " #num ::: CLOBBERS); }
#define TRAP_P(name, num) \
  static inline void name(const void *p) { \
    __asm__ volatile("move.l %0, -(%%sp)\n\t.short " #num :: "r"(p) : CLOBBERS); }

TRAP_P(InitGraf, 0xA86E)
TRAP0(InitFonts, 0xA8FE)
TRAP0(InitWindows, 0xA912)
TRAP0(InitMenus, 0xA930)
TRAP0(TEInit, 0xA9CC)
TRAP_P(InitDialogs, 0xA97B)
TRAP0(InitCursor, 0xA850)
TRAP_P(SetPort, 0xA873)
TRAP_P(DrawString, 0xA884)

static inline void MoveTo(short h, short v) {
  __asm__ volatile("move.w %0, -(%%sp)\n\tmove.w %1, -(%%sp)\n\t.short 0xA893"
                   :: "ri"(h), "ri"(v) : CLOBBERS);
}
static inline unsigned char Button(void) {
  unsigned char r;
  __asm__ volatile("suba.l #2, %%sp\n\t.short 0xA974\n\tmove.b (%%sp), %0\n\t"
                   "adda.l #2, %%sp" : "=d"(r) :: CLOBBERS);
  return r;
}
static inline void FlushEvents(short what, short stop) {
  long arg = ((long)stop << 16) | (unsigned short)what;
  __asm__ volatile("move.l %0, %%d0\n\t.short 0xA032" :: "ri"(arg) : CLOBBERS);
}
/* For calls with many arguments: lay the arguments out in memory exactly
   as they sit on the stack (last argument lowest), copy them below a
   4-byte result slot, and trap. */
#define TRAP_ARGS_L(num, args)                                          \
  ({ long result_;                                                      \
     __asm__ volatile(                                                  \
         "suba.l #4, %%sp\n\t"                                          \
         "suba.l %2, %%sp\n\t"                                          \
         "move.l %%sp, %%a1\n\t"                                        \
         "move.l %1, %%a0\n\t"                                          \
         "move.l %2, %%d0\n\t"                                          \
         "bra 2f\n"                                                     \
         "1:\tmove.b (%%a0)+, (%%a1)+\n"                                \
         "2:\tsub.l #1, %%d0\n\t"                                       \
         "bpl 1b\n\t"                                                   \
         ".short " #num "\n\t"                                          \
         "move.l (%%sp)+, %0"                                           \
         : "=r"(result_) : "r"(&(args)), "i"(sizeof(args)) : CLOBBERS);  \
     result_; })

#pragma options align=mac68k
struct NewWindowArgs {        /* NewWindow's arguments, last first */
  long refCon; unsigned char goAway, pad1; WindowPtr behind; short procID;
  unsigned char visible, pad2; const unsigned char *title; const Rect *r;
  void *storage;
};
#pragma options align=reset
_Static_assert(sizeof(struct NewWindowArgs) == 26, "NewWindowArgs");

static inline WindowPtr NewWindow(void *storage, const Rect *r, const unsigned char *title,
                                  unsigned char visible, short procID, WindowPtr behind,
                                  unsigned char goAway, long refCon) {
  struct NewWindowArgs a = {refCon, goAway, 0, behind, procID, visible, 0, title, r, storage};
  return (WindowPtr)TRAP_ARGS_L(0xA913, a);
}

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
