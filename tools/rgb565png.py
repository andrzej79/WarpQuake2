#!/usr/bin/env python3
"""A 16 bpp frame dump (-crcdump at vid_bpp 16: frame<n>_<w>x<h>.rgb565,
big-endian RGB565) as a PNG:  tools/rgb565png.py <dump> [<out.png>]"""
import re
import struct
import sys
import zlib

src = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else re.sub(r'\.rgb565$', '', src) + '.png'
w, h = map(int, re.search(r'_(\d+)x(\d+)\.rgb565$', src).groups())
data = open(src, 'rb').read()
rows = bytearray()
for y in range(h):
    rows.append(0)
    for x in range(w):
        p = (data[(y * w + x) * 2] << 8) | data[(y * w + x) * 2 + 1]
        r, g, b = p >> 11, (p >> 5) & 63, p & 31
        rows += bytes(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31))


def chunk(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)


open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                      chunk(b'IDAT', zlib.compress(bytes(rows), 9)) + chunk(b'IEND', b''))
print(out)
