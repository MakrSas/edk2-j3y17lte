#!/usr/bin/env python3
"""Replace the kernel in a Samsung header-v0 boot.img (with dt_size field), keep ramdisk/DTB.

usage: repack_boot.py stock_boot.img new_kernel out_boot.img
"""
import hashlib
import struct
import sys

HDR = '<8s10I16s512s32s1024s'


def pad(b, ps):
    return b + b'\0' * ((-len(b)) % ps)


def main(stock, kernel, out):
    d = open(stock, 'rb').read()
    h = list(struct.unpack(HDR, d[:struct.calcsize(HDR)]))
    assert h[0] == b'ANDROID!', 'not an Android boot image'
    ks, rs, ss, ps, dts = h[1], h[3], h[5], h[8], h[9]
    r = lambda x: (x + ps - 1) // ps * ps
    off = ps
    off += r(ks)
    ramdisk = d[off:off + rs]; off += r(rs)
    second = d[off:off + ss]; off += r(ss)
    dt = d[off:off + dts]

    k = open(kernel, 'rb').read()
    h[1] = len(k)
    sha = hashlib.sha1()
    for blob in (k, ramdisk, second, dt):
        sha.update(blob)
        sha.update(struct.pack('<I', len(blob)))
    h[13] = sha.digest().ljust(32, b'\0')

    img = pad(struct.pack(HDR, *h), ps)
    img += pad(k, ps) + pad(ramdisk, ps) + pad(second, ps) + pad(dt, ps)
    img += b'SEANDROIDENFORCE'
    open(out, 'wb').write(img)
    print(f'{out}: {len(img)} bytes, kernel {len(k)}, ramdisk {rs}, dt {dts}, board {h[11].rstrip(b"\0").decode()}')


if __name__ == '__main__':
    main(*sys.argv[1:4])
