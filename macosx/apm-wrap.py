#!/usr/bin/env python3
"""Put an Apple partition map in front of a bare HFS+ volume image.

Some copies of the Mac OS X install DVD are just the HFS+ volume, with no
partition map; QEMU's OpenBIOS cannot boot those. This writes OUT as a
32 KiB partition map followed by the volume:

    python3 apm-wrap.py tiger.iso dvd.img
"""
import os
import shutil
import struct
import sys

src, out = sys.argv[1], sys.argv[2]
blocks = os.path.getsize(src) // 512
start = 64  # the volume starts at 32 KiB


def entry(first, count, name, kind, status):
    b = bytearray(512)
    struct.pack_into(">2sHIII32s32sIII", b, 0, b"PM", 0, 2, first, count,
                     name, kind, 0, count, status)
    return bytes(b)


ddr = bytearray(512)
struct.pack_into(">2sHI", ddr, 0, b"ER", 512, start + blocks)
head = bytes(ddr)
head += entry(1, start - 1, b"Apple", b"Apple_partition_map", 0x37)
head += entry(start, blocks, b"Mac OS X Install DVD", b"Apple_HFS", 0x40000033)
head += b"\0" * (start * 512 - len(head))
with open(out, "wb") as f, open(src, "rb") as s:
    f.write(head)
    shutil.copyfileobj(s, f, 1 << 20)
