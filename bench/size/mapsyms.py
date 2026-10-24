#!/usr/bin/env python3
"""List where the code goes in a Watcom-style map (wlink, JWlink).

Each public symbol's size is taken as the distance to the next symbol in
the same segment, so static functions are counted with the public symbol
before them. Prints the largest library members (modules) and symbols.
usage: mapsyms.py prog.map [count] [code|data|bss]
"""
import collections
import re
import sys


def parse(path):
    segs = []      # (frame, start, end, cls)
    syms = []      # (linear, name, module)
    module = '?'
    in_segs = in_mem = False
    with open(path, errors='replace') as f:
        for ln in f:
            ln = ln.rstrip('\r\n')
            if ln.startswith('Segment') and 'Class' in ln:
                in_segs = True
                continue
            if 'Memory Map' in ln:
                in_segs = False
                in_mem = True
                continue
            if in_segs:
                m = re.match(r'^(\S+)\s+(\S+)\s+(?:(\S+)\s+)?([0-9a-f]{4}):([0-9a-f]{4})\s+([0-9a-f]+)$', ln)
                if m:
                    lin = int(m.group(4), 16) * 16 + int(m.group(5), 16)
                    segs.append((lin, lin + int(m.group(6), 16), m.group(2), m.group(1)))
                continue
            if in_mem:
                if ln.startswith('Module:'):
                    module = ln.split(':', 1)[1].strip()
                    continue
                m = re.match(r'^([0-9a-f]{4}):([0-9a-f]{4})[*+ ]\s*(.*)$', ln)
                if m:
                    lin = int(m.group(1), 16) * 16 + int(m.group(2), 16)
                    syms.append((lin, m.group(3), module))
                if ln.startswith('+') and 'Imported' in ln:
                    break
    return segs, syms


def main():
    path = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 25
    kind = sys.argv[3] if len(sys.argv) > 3 else 'code'
    segs, syms = parse(path)

    def kind_of(cls):
        c = cls.upper()
        if c.endswith('CODE'):
            return 'code'
        if c == 'STACK':
            return 'stack'
        return 'bss' if 'BSS' in c else 'data'
    code = [s for s in segs if kind_of(s[2]) == kind]
    sized = []
    for lo, hi, cls, name in code:
        inseg = sorted(s for s in syms if lo <= s[0] < hi)
        for i, (a, n, mod) in enumerate(inseg):
            end = inseg[i + 1][0] if i + 1 < len(inseg) else hi
            sized.append((end - a, n, mod))
    bymod = collections.Counter()
    for sz, n, mod in sized:
        bymod[mod] += sz
    total = sum(hi - lo for lo, hi, _, _ in code)
    print('%s: %d bytes' % (kind, total))
    print('\nlargest modules:')
    for mod, sz in bymod.most_common(count):
        print('%7d  %s' % (sz, mod))
    print('\nlargest symbols:')
    for sz, n, mod in sorted(sized, reverse=True)[:count]:
        print('%7d  %s  [%s]' % (sz, n[:90], mod.split('(')[-1].rstrip(')')))


if __name__ == '__main__':
    main()
