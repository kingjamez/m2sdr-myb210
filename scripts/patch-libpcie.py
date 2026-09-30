#!/usr/bin/env python3
"""Fix the HamGeek M2SDR libpcie completion deadlock in x64_libpcie.a,
arm_libpcie.a, or a libuhd.so that links either of them.

libpcie's do_cb thread reads the pending-completion count (BAR0 reg 0x1c) and
masks it with 0x3f. Once 64 or more completions are queued the count is
misread (64 -> 0), the queue is never drained, every DMA descriptor is used
up and RX data plus control replies stop: Receiver TIMEOUT, then
wait_for_ack. This widens the mask to 0x1ff (the library has at most 192
descriptors in flight).

Usage: patch_libpcie.py FILE [FILE...]   (edits in place; keep a backup)
"""
import re
import sys

PATCHES = {
    # x86_64: and dword [rbp-0x1c],0x3f; cmp dword [rbp-0x1c],0; je +0xc1; jmp +0xa0
    #      -> mov eax,[rbp-0x1c]; and eax,0x1ff; mov [rbp-0x1c],eax; jmp (same loop); nop*3
    "x86_64": (
        bytes.fromhex("8365e43f837de4000f84c1000000e9a0000000"),
        bytes.fromhex("8b45e4" "25ff010000" "8945e4" "e9a3000000" "909090"),
    ),
    # aarch64: ldr w0,[sp,#60]; and w0,w0,#0x3f; str w0,[sp,#60]
    #       -> ldr w0,[sp,#60]; and w0,w0,#0x1ff; str w0,[sp,#60]
    "aarch64": (
        bytes.fromhex("e03f40b9" "00140012" "e03f00b9"),
        bytes.fromhex("e03f40b9" "00200012" "e03f00b9"),
    ),
}

status = 0
for path in sys.argv[1:]:
    data = bytearray(open(path, "rb").read())
    done = False
    for arch, (old, new) in PATCHES.items():
        if data.count(new) == 1 and data.count(old) == 0:
            print(f"{path}: already patched ({arch})")
            done = True
            break
        hits = [m.start() for m in re.finditer(re.escape(old), data)]
        if len(hits) == 1:
            data[hits[0]:hits[0] + len(old)] = new
            open(path, "wb").write(data)
            print(f"{path}: patched ({arch}) at {hex(hits[0])}")
            done = True
            break
        if len(hits) > 1:
            print(f"{path}: {arch} pattern found {len(hits)} times, refusing")
            status = 1
            done = True
            break
    if not done:
        print(f"{path}: do_cb pattern not found (different libpcie build?)")
        status = 1
sys.exit(status)
