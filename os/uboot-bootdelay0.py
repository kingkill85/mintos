#!/usr/bin/env python3
"""Set U-Boot's bootdelay from 2 to 0 in the SD card's u-boot.itb (2 s faster boot).

The FIT at sector 16384 keeps U-Boot as external data with a SHA-256 the SPL checks, so the
patch changes the one byte and writes the new hash into the FIT header. Usage:
  ssh root@rg353v 'dd if=/dev/mmcblk1 bs=512 skip=16384 count=8192' > itb-area.bin
  os/uboot-bootdelay0.py itb-area.bin itb-area.patched     # prints the sectors to write back
Verified 2026-11-08 on the ROCKNIX U-Boot 2026.01 build; original kept in
/storage/.backup/uboot-itb-area.orig on the device.
"""
import hashlib
import sys

src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, "rb").read())
assert d[:4] == b"\xd0\x0d\xfe\xed", "no FIT at the start"
hdr = int.from_bytes(d[4:8], "big")            # external data starts after the FIT header
ub_start = hdr
i = d.find(b"bootdelay=2\x00", hdr)
assert i > 0, "bootdelay=2 not found (already patched?)"
# u-boot is the first image (data-offset 0); its data-size is the first 'data-size'
# property in the FIT structure block, read with a tiny FDT walk
def fdt_props(blob):
    off_struct = int.from_bytes(blob[8:12], "big"); off_strings = int.from_bytes(blob[12:16], "big")
    p = off_struct
    while True:
        tag = int.from_bytes(blob[p:p + 4], "big"); p += 4
        if tag == 1:                                   # BEGIN_NODE
            while blob[p] != 0: p += 1
            p = (p + 4) & ~3
        elif tag == 3:                                 # PROP
            ln = int.from_bytes(blob[p:p + 4], "big"); no = int.from_bytes(blob[p + 4:p + 8], "big"); p += 8
            name = blob[off_strings + no:blob.index(b"\0", off_strings + no)].decode()
            yield name, p, bytes(blob[p:p + ln])
            p = (p + ln + 3) & ~3
        elif tag in (2, 4): continue                  # END_NODE, NOP
        else: return
props = list(fdt_props(d))
size = next(int.from_bytes(v, "big") for n, _, v in props if n == "data-size")
old = hashlib.sha256(bytes(d[ub_start:ub_start + size])).digest()
hpos = bytes(d[:hdr]).find(old)
assert hpos > 0, "u-boot hash not found in the header"
d[i + 10] = ord("0")
d[hpos:hpos + 32] = hashlib.sha256(bytes(d[ub_start:ub_start + size])).digest()
open(dst, "wb").write(d)
print(f"patched; write back sectors {16384 + hpos // 512} and {16384 + (i + 10) // 512}")
