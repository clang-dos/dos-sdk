/* Calls the resident program's INT 60h, after filling all the free DOS
 * memory, which would overwrite the program if it kept too little. */
#include <dos.h>
#include <stdio.h>

int main(void) {
  /* DOS gives a program all of its memory: keep the PSP up to the end of
   * DGROUP (this is a small-model program). */
  unsigned ds;
  __asm__("movw %%ds, %0" : "=r"(ds));
  unsigned keep = ds - _psp + 0x1000, r = 0x4a00;
  __asm__ volatile("pushw %%es\n\tmovw %2, %%es\n\tint $0x21\n\tpopw %%es"
                   : "+a"(r), "+b"(keep)
                   : "r"(_psp)
                   : "cx", "dx", "memory");
  /* Wait for 3 clock ticks, which the resident program counts. */
  volatile unsigned long __far *clock = MK_FP(0x40, 0x6c);
  unsigned long t0 = *clock;
  while (*clock - t0 < 3)
    ;
  unsigned ax = 0x4800, bx = 0xffff, seg;
  /* The largest free block: BX paragraphs. */
  __asm__ volatile("int $0x21" : "+a"(ax), "+b"(bx) : : "cx", "dx", "memory");
  ax = 0x4800;
  unsigned paras = bx;
  __asm__ volatile("int $0x21" : "+a"(ax), "+b"(bx) : : "cx", "dx", "memory");
  seg = ax;
  /* Fill it with INT3 bytes, 16 at a time. */
  for (unsigned i = 0; i < paras; i++)
    __asm__ volatile("pushw %%es\n\tmovw %0, %%es\n\txorw %%di, %%di\n\t"
                     "movw $8, %%cx\n\tmovw $0xcccc, %%ax\n\tcld\n\t"
                     "rep stosw\n\tpopw %%es"
                     :
                     : "r"(seg + i)
                     : "ax", "cx", "di", "memory");
  ax = 0x4900;
  __asm__ volatile("pushw %%es\n\tmovw %1, %%es\n\tint $0x21\n\tpopw %%es"
                   : "+a"(ax)
                   : "r"(seg)
                   : "bx", "cx", "dx", "memory");

  unsigned calls, ticks;
  ax = 0x5453;
  __asm__ volatile("int $0x60"
                   : "+a"(ax), "=b"(calls), "=c"(ticks)
                   :
                   : "dx", "memory");
  printf("ax=%u calls=%u ticks=%u filled=%u\n", ax, calls, ticks, paras);
  return 0;
}
