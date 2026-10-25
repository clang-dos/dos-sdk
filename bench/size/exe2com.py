#!/usr/bin/env python3
"""Turn a tiny-model MZ file into a .COM file, as EXE2BIN does.

Digital Mars links tiny programs as an .EXE and runs the DOS EXE2BIN on it;
this does the same on any host. It fails when the .EXE has relocations,
which a .COM program cannot have.
usage: exe2com.py in.exe out.com
"""
import struct
import sys

d = open(sys.argv[1], 'rb').read()
if d[:2] != b'MZ':
    sys.exit('exe2com: not an MZ file')
last, pages, nrel, hdr = struct.unpack_from('<4H', d, 2)
ip, cs = struct.unpack_from('<2H', d, 20)
size = pages * 512 - (512 - last if last else 0)
if nrel:
    sys.exit('exe2com: %d relocations, cannot be a .COM program' % nrel)
if cs != 0 or ip != 0x100:
    sys.exit('exe2com: entry point is not 0:100')
image = d[hdr * 16:size]
open(sys.argv[2], 'wb').write(image[0x100:])
