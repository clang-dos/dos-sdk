#!/usr/bin/env python3
"""Check a 16-bit Windows (NE) program or DLL for the Windows versions it
claims to run on.

    ne-check.py [--version X.YY] [--real-mode | --protmode] FILE...

Prints the expected Windows version, the protected-mode-only flag, the
modules it imports from and each segment's flags, then checks:

  --version X.YY  the header expects this Windows version (3.10 for 3.1).
  --protmode      the header marks the program for protected mode only, so
                  Windows 3.0 refuses to run it in real mode.
  --real-mode     the program can run in real mode (Windows 1.x, 2.x, and
                  3.0 in real mode): not marked protected-mode only, every
                  code segment fixed, and no relocation that real-mode
                  Windows cannot keep up to date when segments move: a
                  separate segment or offset of an imported function, or of
                  a moveable segment of the program.

Exits with 1 if a check fails.
"""
import argparse
import struct
import sys

SEG_DATA = 0x0001
SEG_MOVEABLE = 0x0010
SEG_RELOCS = 0x0100
SEG_DISCARDABLE = 0x1000
PROTMODE = 0x0008

# Modules that Windows always has loaded.
SYSTEM = {"KERNEL", "USER", "GDI", "KEYBOARD", "SOUND", "SYSTEM", "DISPLAY",
          "MOUSE", "COMM"}

# System DLLs that Windows 3.1 introduced (some were redistributable for 3.0).
AFTER_30 = {"COMMDLG", "DDEML", "LZEXPAND", "MMSYSTEM", "OLECLI", "OLESVR",
            "SHELL", "TOOLHELP", "VER"}

ADDR_TYPES = {0: "lobyte", 2: "segment", 3: "far pointer", 5: "offset",
              11: "pointer48", 13: "offset32"}
REL_TYPES = {0: "internal", 1: "import ordinal", 2: "import name",
             3: "osfixup"}


def parse(path):
    data = open(path, "rb").read()
    if data[:2] != b"MZ":
        raise ValueError("not an MZ executable")
    ne = struct.unpack_from("<I", data, 0x3C)[0]
    if data[ne:ne + 2] != b"NE":
        raise ValueError("not an NE executable")
    flags = struct.unpack_from("<H", data, ne + 0x0C)[0]
    nseg = struct.unpack_from("<H", data, ne + 0x1C)[0]
    segtab = ne + struct.unpack_from("<H", data, ne + 0x22)[0]
    shift = struct.unpack_from("<H", data, ne + 0x32)[0] or 9
    expver = struct.unpack_from("<H", data, ne + 0x3E)[0]
    ncmod = struct.unpack_from("<H", data, ne + 0x1E)[0]
    modtab = ne + struct.unpack_from("<H", data, ne + 0x28)[0]
    imptab = ne + struct.unpack_from("<H", data, ne + 0x2A)[0]
    modules = []
    for i in range(ncmod):
        o = imptab + struct.unpack_from("<H", data, modtab + 2 * i)[0]
        modules.append(data[o + 1:o + 1 + data[o]].decode("latin-1"))
    segs = []
    for i in range(nseg):
        off, length, sflags, minalloc = struct.unpack_from(
            "<HHHH", data, segtab + 8 * i)
        relocs = []
        if sflags & SEG_RELOCS and off:
            start = off << shift
            size = length or (0x10000 if off else 0)
            pos = start + size
            count = struct.unpack_from("<H", data, pos)[0]
            for j in range(count):
                atype, rtype, at, a, b = struct.unpack_from(
                    "<BBHHH", data, pos + 2 + 8 * j)
                relocs.append((atype, rtype, at, a, b))
        segs.append((sflags, length, relocs))
    return flags, expver, segs, modules


def describe(rel):
    atype, rtype, at, a, b = rel
    kind = rtype & 3
    what = ADDR_TYPES.get(atype, f"type {atype}")
    if kind == 0:
        seg = a & 0xFF
        target = (f"entry {b} (moveable)" if seg == 0xFF
                  else f"segment {seg}:{b:04x}")
    elif kind == 1:
        target = f"import module {a} ordinal {b}"
    elif kind == 2:
        target = f"import module {a} name @{b}"
    else:
        target = "osfixup"
    return f"{what} at {at:04x} -> {target}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--real-mode", action="store_true")
    mode.add_argument("--protmode", action="store_true")
    ap.add_argument("files", nargs="+")
    args = ap.parse_args()
    bad = 0
    for path in args.files:
        problems = []
        try:
            flags, expver, segs, modules = parse(path)
        except (ValueError, struct.error) as e:
            print(f"{path}: {e}")
            bad += 1
            continue
        ver = f"{expver >> 8}.{expver & 0xFF:02d}"
        print(f"{path}: expects Windows {ver}"
              f"{', protected mode only' if flags & PROTMODE else ''}")
        print(f"  imports from: {', '.join(modules) or 'nothing'}")
        if expver < 0x30A:
            late = [m for m in modules if m.upper() in AFTER_30]
            if late:
                print(f"  note: {', '.join(late)} came with Windows 3.1; "
                      "Windows 3.0 and older have them only if installed")
        own = [m for m in modules if m.upper() not in SYSTEM]
        if expver < 0x300 and own:
            print(f"  note: before Windows 3.0, Windows loads an imported "
                  f"module from MODULE.EXE, so {', '.join(own)} must be "
                  f"named {', '.join(m.upper() + '.EXE' for m in own)}")
        for i, (sflags, length, relocs) in enumerate(segs, 1):
            kind = "data" if sflags & SEG_DATA else "code"
            attrs = ["moveable" if sflags & SEG_MOVEABLE else "fixed"]
            if sflags & SEG_DISCARDABLE:
                attrs.append("discardable")
            print(f"  segment {i}: {kind}, {' '.join(attrs)}, "
                  f"{length or 0x10000} bytes, {len(relocs)} relocations")
        if args.version:
            major, _, minor = args.version.partition(".")
            want = (int(major) << 8) | int(minor or 0)
            if expver != want:
                problems.append(f"expects Windows {ver}, not {args.version}")
        if args.protmode and not flags & PROTMODE:
            problems.append("not marked protected mode only")
        if args.real_mode:
            if flags & PROTMODE:
                problems.append("marked protected mode only")
            moveable = {i for i, s in enumerate(segs, 1)
                        if s[0] & SEG_MOVEABLE}

            def unsafe(rel):
                atype, rtype, _, a, _ = rel
                # A far pointer goes through the target's entry, which
                # Windows keeps up to date; a separate segment or offset
                # does not.
                if atype not in (2, 5):
                    return False
                kind = rtype & 3
                if kind in (1, 2):
                    return True
                seg = a & 0xFF
                return kind == 0 and (seg == 0xFF or seg in moveable)

            for i, (sflags, _, relocs) in enumerate(segs, 1):
                if not sflags & SEG_DATA and sflags & SEG_MOVEABLE:
                    problems.append(f"code segment {i} is moveable")
                for rel in relocs:
                    if unsafe(rel):
                        problems.append(f"segment {i}: {describe(rel)}")
        for p in problems:
            print(f"  FAIL: {p}")
        if problems:
            bad += 1
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
