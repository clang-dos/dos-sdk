#!/usr/bin/env python3
"""Write linker import directives for the exports of a 16-bit Windows DLL.

    win16-imports.py MYDLL.DLL > mydll.lnk
    clang --target=i8086-pc-win16 app.c -Wl,@mydll.lnk

Reads the NE file's resident and nonresident name tables and prints one
wlink directive per exported name, by ordinal:

    import ADDNUMBERS MYDLL.2
"""
import struct
import sys


def names(data, off, end=None):
    """The (name, ordinal) entries of an NE name table at off. The first
    entry is the module name (resident) or description (nonresident)."""
    out = []
    while end is None or off < end:
        n = data[off]
        if n == 0:
            break
        name = data[off + 1:off + 1 + n].decode("latin-1")
        (ordinal,) = struct.unpack_from("<H", data, off + 1 + n)
        out.append((name, ordinal))
        off += 3 + n
    return out


def main():
    data = open(sys.argv[1], "rb").read()
    (ne,) = struct.unpack_from("<I", data, 0x3C)
    if data[ne:ne + 2] != b"NE":
        sys.exit(f"{sys.argv[1]}: not an NE file")
    restab, = struct.unpack_from("<H", data, ne + 0x26)
    nrestab, = struct.unpack_from("<I", data, ne + 0x2C)
    resident = names(data, ne + restab)
    nonresident = names(data, nrestab) if nrestab else []
    module = resident[0][0]
    for name, ordinal in resident[1:] + nonresident[1:]:
        if name.upper() == "WEP":
            continue
        print(f"import {name} {module}.{ordinal}")


if __name__ == "__main__":
    main()
