/* __eprintf for PowerPC Mac OS X 10.0 to 10.3, whose <assert.h> calls it.
 * Apple's GCC got it from libcc_dynamic.a or libgcc.a; compiler-rt's
 * eprintf.c needs the SDK's <stdio.h>, so this one only uses what every
 * libSystem from 10.0 on exports. */
typedef __SIZE_TYPE__ size_t;
extern int snprintf(char *, size_t, const char *, ...);
extern long write(int, const void *, size_t);
extern void abort(void) __attribute__((noreturn));

__attribute__((visibility("hidden"), noreturn)) void
__eprintf(const char *format, const char *expression, unsigned line,
          const char *file) {
  char buf[512];
  int n = snprintf(buf, sizeof buf, format, expression, line, file);
  if (n > 0)
    write(2, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
  abort();
}
