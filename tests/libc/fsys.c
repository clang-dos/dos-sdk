#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>

/* File system calls: opendir and readdir, stat on directories, realpath,
 * chmod, truncate, utime, statvfs and the missing links. Prints
 * ALL PASSED. */
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s (errno %d)\n", __LINE__, #c, errno); fails++; } } while (0)

static void
put(const char *name, const char *s)
{
    FILE *f = fopen(name, "wb");
    fputs(s, f);
    fclose(f);
}

int
main(void)
{
    struct stat st;
    rmdir("FSD\\SUB");
    unlink("FSD\\ONE.TXT");
    unlink("FSD\\TWO.TXT");
    rmdir("FSD");
    CHECK(mkdir("FSD", 0777) == 0);
    CHECK(mkdir("FSD\\SUB", 0777) == 0);
    put("FSD\\ONE.TXT", "12345");
    put("FSD\\TWO.TXT", "ab");

    DIR *d = opendir("FSD");
    CHECK(d != NULL);
    int files = 0, dirs = 0, dots = 0;
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            dots++;
        else if (e->d_type == DT_DIR && !strcmp(e->d_name, "SUB"))
            dirs++;
        else if (e->d_type == DT_REG && (!strcmp(e->d_name, "ONE.TXT") || !strcmp(e->d_name, "TWO.TXT")))
            files++;
        else
            printf("unexpected entry %s\n", e->d_name);
    }
    CHECK(files == 2 && dirs == 1);
    if (d) {
        rewinddir(d);
        int n = 0;
        while (readdir(d))
            n++;
        CHECK(n == 3 + dots);
        CHECK(closedir(d) == 0);
    }
    d = opendir("FSD\\SUB\\");
    CHECK(d != NULL);
    while (d && (e = readdir(d)) != NULL)
        CHECK(!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."));
    if (d)
        closedir(d);
    errno = 0;
    CHECK(opendir("NOWHERE") == NULL && errno == ENOENT);
    errno = 0;
    CHECK(opendir("FSD\\ONE.TXT") == NULL && errno == ENOTDIR);
    d = opendir("\\");
    CHECK(d != NULL);
    if (d)
        closedir(d);

    CHECK(stat("FSD\\", &st) == 0 && S_ISDIR(st.st_mode));
    CHECK(stat("FSD\\SUB\\..", &st) == 0 && S_ISDIR(st.st_mode));
    CHECK(stat(".", &st) == 0 && S_ISDIR(st.st_mode));
    CHECK(lstat("FSD\\ONE.TXT", &st) == 0 && S_ISREG(st.st_mode) && st.st_size == 5);
    CHECK(stat("FSD\\NONE", &st) < 0 && errno == ENOENT);

    char buf[PATH_MAX];
    char cwd[PATH_MAX];
    CHECK(getcwd(cwd, sizeof(cwd)) != NULL);
    CHECK(realpath("FSD\\SUB\\..\\ONE.TXT", buf) != NULL);
    char want[PATH_MAX];
    snprintf(want, sizeof(want), "%s%sFSD\\ONE.TXT", cwd, cwd[strlen(cwd) - 1] == '\\' ? "" : "\\");
    CHECK(!strcmp(buf, want));
    CHECK(realpath("FSD\\NONE", buf) == NULL && errno == ENOENT);

    /* emu2 accepts but ignores the read-only attribute and file times
     * (INT 21h functions 4301h and 5701h); build with -DEMU2 to check
     * only that the calls succeed. */
    CHECK(chmod("FSD\\TWO.TXT", 0444) == 0);
#ifndef EMU2
    CHECK(stat("FSD\\TWO.TXT", &st) == 0 && !(st.st_mode & 0200));
#endif
    CHECK(chmod("FSD\\TWO.TXT", 0666) == 0);
    CHECK(stat("FSD\\TWO.TXT", &st) == 0 && (st.st_mode & 0200));

    CHECK(truncate("FSD\\ONE.TXT", 2) == 0);
    CHECK(stat("FSD\\ONE.TXT", &st) == 0 && st.st_size == 2);
    int fd = open("FSD\\ONE.TXT", O_RDWR | O_BINARY);
    CHECK(fd >= 0 && ftruncate(fd, 10) == 0);
    CHECK(fstat(fd, &st) == 0 && st.st_size == 10);
    close(fd);

    struct utimbuf ut = { 0, 1000000000 }; /* 2001-09-09 01:46:40 */
    CHECK(utime("FSD\\ONE.TXT", &ut) == 0);
#ifndef EMU2
    CHECK(stat("FSD\\ONE.TXT", &st) == 0 && st.st_mtime == 1000000000);
#endif
    struct timeval tv[2] = { { 0, 0 }, { 1500000000, 0 } }; /* 2017-07-14 02:40:00 */
    CHECK(utimes("FSD\\ONE.TXT", tv) == 0);
#ifndef EMU2
    CHECK(stat("FSD\\ONE.TXT", &st) == 0 && st.st_mtime == 1500000000);
#endif

    struct statvfs vfs;
    CHECK(statvfs("FSD", &vfs) == 0 && vfs.f_frsize > 0 && vfs.f_blocks > 0 && vfs.f_bavail <= vfs.f_blocks);
    CHECK(link("FSD\\ONE.TXT", "FSD\\L.TXT") < 0 && errno == ENOSYS);
    CHECK(readlink("FSD\\ONE.TXT", buf, sizeof(buf)) < 0 && errno == EINVAL);

    CHECK(unlink("FSD\\ONE.TXT") == 0 && unlink("FSD\\TWO.TXT") == 0);
    CHECK(rmdir("FSD\\SUB") == 0 && rmdir("FSD") == 0);
    CHECK(opendir("FSD") == NULL);
    puts(fails ? "FAILED" : "ALL PASSED");
    return fails;
}
