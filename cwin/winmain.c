/* main for programs that define WinMain, as for Windows: the command
 * line is the arguments after the program name, joined by spaces.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "cwin-local.h"
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
    size_t n = 1;
    for (int i = 1; i < argc; i++)
        n += strlen(argv[i]) + 1;
    char *cmd = (char *)malloc(n);
    if (!cmd)
        return 1;
    cmd[0] = 0;
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            strcat(cmd, " ");
        strcat(cmd, argv[i]);
    }
    int r = WinMain(0, 0, cmd, SW_SHOWNORMAL);
    CwShutdown();
    return r;
}
