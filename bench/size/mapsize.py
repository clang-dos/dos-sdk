#!/usr/bin/env python3
"""Sum code, data and BSS from a linker map file.

Reads the segment table of a Watcom-style map (Open Watcom wlink, JWlink),
an OPTLINK/Microsoft-style map (Digital Mars), or a GNU ld map (gcc-ia16),
and prints "code data bss" in bytes. With --com (a .COM program), the
256 bytes before the entry point at 100h are taken out of the code when the
code segment starts at offset 0, as it does in Watcom and OPTLINK maps. The stack is left out, since each
toolchain picks its own default.

Code is every segment whose class ends in CODE (CODE, FAR_CODE, ...).
BSS is every segment whose class names BSS. Everything else in the load
image (DATA, CONST, BEGDATA, FAR_DATA, vtables, ...) is data.
"""
import re
import sys


def classify(cls):
    c = cls.upper()
    if c == 'STACK':
        return None
    if c.endswith('CODE'):
        return 'code'
    if 'BSS' in c:
        return 'bss'
    return 'data'


def watcom(lines):
    out = {'code': 0, 'data': 0, 'bss': 0}
    in_table = False
    found = False
    for ln in lines:
        if ln.startswith('Segment') and 'Class' in ln and 'Size' in ln:
            in_table = True
            continue
        if in_table:
            if ln.startswith('='):
                continue
            if not ln.strip():
                if found:
                    break
                continue
            f = ln.split()
            # name class [group] address size
            if len(f) < 4 or not re.fullmatch(r'[0-9a-fA-F]+', f[-1]):
                continue
            k = classify(f[1])
            if k:
                out[k] += int(f[-1], 16)
            found = True
    return out if found else None


def optlink(lines):
    out = {'code': 0, 'data': 0, 'bss': 0}
    found = False
    pat = re.compile(r'^\s*([0-9A-F]+)H\s+([0-9A-F]+)H\s+([0-9A-F]+)H\s+(\S+)\s+(\S+)')
    for ln in lines:
        m = pat.match(ln)
        if not m:
            if found and not ln.strip():
                break
            continue
        k = classify(m.group(5))
        if k:
            out[k] += int(m.group(3), 16)
        found = True
    return out if found else None


def gnu(lines):
    """Top-level output sections of a GNU ld map."""
    out = {'code': 0, 'data': 0, 'bss': 0}
    found = False
    pending = None
    started = False
    for ln in lines:
        if ln.startswith('Linker script and memory map'):
            started = True
            continue
        if not started:
            continue
        m = re.match(r'^(\.[\w.$]+)\s*$', ln)
        if m:
            pending = m.group(1)
            continue
        m = re.match(r'^(\.[\w.$]+)?\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)', ln)
        if m and (m.group(1) or (pending and ln[0] == ' ')):
            name = m.group(1) or pending
            pending = None
            size = int(m.group(3), 16)
            n = name.lower()
            if n.startswith(('.debug', '.comment', '.note', '.stab', '.msdos_mz', '.reloc', '.ia16')):
                continue
            if 'stack' in n:
                continue
            if 'text' in n or n.startswith(('.init', '.fini')):
                out['code'] += size
            elif 'bss' in n:
                out['bss'] += size
            else:
                out['data'] += size
            found = True
            continue
        pending = None
    return out if found else None


def main():
    args = sys.argv[1:]
    com = '--com' in args
    args = [a for a in args if a != '--com']
    with open(args[0], errors='replace') as f:
        lines = [l.rstrip('\r\n') for l in f]
    text = '\n'.join(lines[:200])
    gnu_map = 'Linker script and memory map' in '\n'.join(lines)
    if gnu_map:
        r = gnu(lines)
    elif 'Segment' in text and 'Class' in text and 'Group' in text:
        r = watcom(lines)
    else:
        r = optlink(lines)
    if r is None:
        sys.exit('mapsize: no segment table in ' + args[0])
    if com and not gnu_map:
        r['code'] -= 0x100
    print(r['code'], r['data'], r['bss'])


if __name__ == '__main__':
    main()
