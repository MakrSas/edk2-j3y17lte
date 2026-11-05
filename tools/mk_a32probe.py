#!/usr/bin/env python3
# Polyglot kernel probe. Samsung sboot entering in AArch32 -> screen GREEN,
# entering in AArch64 -> screen RED.  usage: mk_a32probe.py out.bin [arm64magic]
import struct, sys
img = bytearray(0x1000)
def w(off, v): struct.pack_into('<I', img, off, v & 0xffffffff)
FB, PIX, DECON, DVAL = 0x67000000, 720*1280, 0x14830070, 0x1281
# word0: A32 "b 0x40"; in A64 it decodes as "ands x14, x0, x0"
w(0x00, 0xEA000000 | ((0x40 - 8) // 4))
# word1: A64 "b 0x80"
w(0x04, 0x14000000 | ((0x80 - 4) // 4))
w(0x24, 0x016F2818); w(0x28, 0); w(0x2C, len(img))   # zImage magic/start/end
if len(sys.argv) > 2:
    struct.pack_into('<Q', img, 0x08, 0x80000); struct.pack_into('<Q', img, 0x10, len(img))
    w(0x38, 0x644D5241)                                  # "ARMd"
# ---- A32 code @0x40, literals @0x68
L = 0x68
def ldr32(pc, rt, lit): return 0xE59F0000 | (rt << 12) | (lit - (pc + 8))
w(0x40, ldr32(0x40, 3, L+0)); w(0x44, ldr32(0x44, 4, L+4)); w(0x48, 0xE5834000)  # str r4,[r3]
w(0x4C, ldr32(0x4C, 0, L+8)); w(0x50, ldr32(0x50, 1, L+12)); w(0x54, ldr32(0x54, 2, L+16))
w(0x58, 0xE4801004)   # str r1,[r0],#4
w(0x5C, 0xE2522001)   # subs r2,r2,#1
w(0x60, 0x1AFFFFFC)   # bne 0x58
w(0x64, 0xEAFFFFFE)   # b .
for i, v in enumerate([DECON, DVAL, FB, 0xFF00FF00, PIX]): w(L + 4*i, v)
# ---- A64 code @0x80, literals @0xB0
L = 0xB0
def ldr64(pc, rt, lit): return 0x18000000 | (((lit - pc) // 4) << 5) | rt   # ldr wT, lit
w(0x80, ldr64(0x80, 3, L+0)); w(0x84, ldr64(0x84, 4, L+4)); w(0x88, 0xB9000064)  # str w4,[x3]
w(0x8C, ldr64(0x8C, 0, L+8)); w(0x90, ldr64(0x90, 1, L+12)); w(0x94, ldr64(0x94, 2, L+16))
w(0x98, 0xB8004401)   # str w1,[x0],#4
w(0x9C, 0x71000442)   # subs w2,w2,#1
w(0xA0, 0x54FFFFC1)   # b.ne 0x98
w(0xA4, 0x14000000)   # b .
for i, v in enumerate([DECON, DVAL, FB, 0xFFFF0000, PIX]): w(L + 4*i, v)
open(sys.argv[1], 'wb').write(img)
