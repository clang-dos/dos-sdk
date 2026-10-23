/* Text and binary files: CR LF line ends and Ctrl-Z in text mode, raw bytes
 * in binary mode, O_TEXT, O_BINARY, _fmode and setmode. Prints ALL PASSED. */
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static size_t slurp(const char *name, const char *mode, char *buf, size_t size) {
  FILE *f = fopen(name, mode);
  if (!f)
    return (size_t)-1;
  size_t n = fread(buf, 1, size, f);
  fclose(f);
  return n;
}

static void put_raw(const char *name, const char *data, size_t n) {
  int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  write(fd, data, n);
  close(fd);
}

int main(void) {
  char buf[64];
  size_t n;

  /* Text writes turn LF into CR LF; text reads turn it back. */
  FILE *f = fopen("TEXT.TXT", "w");
  CHECK(f != NULL);
  fputs("one\ntwo\n", f);
  fclose(f);
  n = slurp("TEXT.TXT", "rb", buf, sizeof(buf));
  CHECK(n == 10 && memcmp(buf, "one\r\ntwo\r\n", 10) == 0);
  n = slurp("TEXT.TXT", "r", buf, sizeof(buf));
  CHECK(n == 8 && memcmp(buf, "one\ntwo\n", 8) == 0);
  /* ftell gives file offsets, which fseek returns to. */
  f = fopen("TEXT.TXT", "r");
  CHECK(fgets(buf, sizeof(buf), f) && strcmp(buf, "one\n") == 0);
  long at = ftell(f);
  CHECK(at == 5);
  CHECK(fgets(buf, sizeof(buf), f) && strcmp(buf, "two\n") == 0);
  CHECK(ftell(f) == 10);
  CHECK(fseek(f, at, SEEK_SET) == 0 && fgetc(f) == 't');
  fclose(f);
  /* Switching from reading to writing keeps the place. */
  f = fopen("TEXT.TXT", "r+");
  CHECK(fgets(buf, sizeof(buf), f) && fseek(f, 0, SEEK_CUR) == 0);
  fputs("TWO\n", f);
  fclose(f);
  n = slurp("TEXT.TXT", "rb", buf, sizeof(buf));
  CHECK(n == 10 && memcmp(buf, "one\r\nTWO\r\n", 10) == 0);

  /* Binary writes are raw. "wt" is text, like "w". */
  f = fopen("BIN.DAT", "wb");
  fputs("a\nb\n", f);
  fclose(f);
  n = slurp("BIN.DAT", "rb", buf, sizeof(buf));
  CHECK(n == 4 && memcmp(buf, "a\nb\n", 4) == 0);
  f = fopen("BIN.DAT", "wt");
  fputs("a\n", f);
  fclose(f);
  n = slurp("BIN.DAT", "rb", buf, sizeof(buf));
  CHECK(n == 3 && memcmp(buf, "a\r\n", 3) == 0);

  /* A lone CR stays; Ctrl-Z ends a text file, not a binary one. */
  put_raw("Z.TXT", "x\r\ny\rz\x1aJUNK", 11);
  n = slurp("Z.TXT", "r", buf, sizeof(buf));
  CHECK(n == 5 && memcmp(buf, "x\ny\rz", 5) == 0);
  n = slurp("Z.TXT", "rb", buf, sizeof(buf));
  CHECK(n == 11);
  int fd = open("Z.TXT", O_RDONLY | O_TEXT);
  CHECK(read(fd, buf, sizeof(buf)) == 5);
  CHECK(read(fd, buf, sizeof(buf)) == 0); /* Still at the end. */
  CHECK(lseek(fd, 0, SEEK_CUR) == 6);     /* Left at the Ctrl-Z. */
  CHECK(lseek(fd, 7, SEEK_SET) == 7);     /* Seeking reads again. */
  CHECK(read(fd, buf, sizeof(buf)) == 4 && memcmp(buf, "JUNK", 4) == 0);
  close(fd);

  /* A CR LF split across two reads still reads as LF. */
  fd = open("Z.TXT", O_RDONLY | O_TEXT);
  CHECK(read(fd, buf, 2) == 2 && memcmp(buf, "x\n", 2) == 0);
  CHECK(read(fd, buf, 1) == 1 && buf[0] == 'y');
  close(fd);

  /* open() follows _fmode when given neither O_TEXT nor O_BINARY. */
  CHECK(_fmode == O_TEXT);
  fd = open("Z.TXT", O_RDONLY);
  CHECK(read(fd, buf, sizeof(buf)) == 5);
  CHECK(setmode(fd, O_BINARY) == O_TEXT);
  CHECK(lseek(fd, 0, SEEK_SET) == 0);
  CHECK(read(fd, buf, sizeof(buf)) == 11);
  CHECK(setmode(fd, O_TEXT) == O_BINARY);
  CHECK(setmode(fd, 42) == -1);
  close(fd);
  CHECK(setmode(fd, O_TEXT) == -1); /* Closed. */
  _fmode = O_BINARY;
  fd = open("Z.TXT", O_RDONLY);
  CHECK(read(fd, buf, sizeof(buf)) == 11);
  close(fd);
  _fmode = O_TEXT;

  /* Large text writes go out in pieces; the count is of the caller's bytes. */
  static char big[1000];
  for (int i = 0; i < 1000; i++)
    big[i] = i % 10 == 9 ? '\n' : 'a' + i % 10;
  fd = open("BIG.TXT", O_WRONLY | O_CREAT | O_TRUNC | O_TEXT, 0666);
  CHECK(write(fd, big, 1000) == 1000);
  close(fd);
  fd = open("BIG.TXT", O_RDONLY | O_BINARY);
  CHECK(lseek(fd, 0, SEEK_END) == 1100);
  close(fd);
  static char back[1100];
  n = slurp("BIG.TXT", "r", back, sizeof(back));
  CHECK(n == 1000 && memcmp(back, big, 1000) == 0);

  /* stdout starts in text mode. */
  CHECK(setmode(1, O_TEXT) == O_TEXT);

  unlink("TEXT.TXT");
  unlink("BIN.DAT");
  unlink("Z.TXT");
  unlink("BIG.TXT");
  if (!fails)
    printf("ALL PASSED\n");
  return fails != 0;
}
