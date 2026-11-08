#!/usr/bin/env python3
"""
Dagor Engine 6.5 VROMFS Packer (vromfsPacker-compatible virtual ROM filesystem builder)
Packs application.blk, prog/gameBase/, and prog/scripts/ into game/res/gamedata.vromfs.bin.
"""
import os
import struct
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_BIN = os.path.join(ROOT, "game", "res", "gamedata.vromfs.bin")

def collect_files():
    entries = []
    app_blk = os.path.join(ROOT, "application.blk")
    if os.path.isfile(app_blk):
        entries.append(("application.blk", app_blk))

    for mount_prefix, rel_dir in [("", os.path.join("prog", "gameBase")),
                                  ("scripts", os.path.join("prog", "scripts"))]:
        abs_dir = os.path.join(ROOT, rel_dir)
        if not os.path.isdir(abs_dir):
            continue
        for dp, _, fns in os.walk(abs_dir):
            for fn in sorted(fns):
                full = os.path.join(dp, fn)
                sub = os.path.relpath(full, abs_dir).replace("\\", "/")
                vpath = f"{mount_prefix}/{sub}" if mount_prefix else sub
                entries.append((vpath, full))
    return entries

def build_vromfs():
    os.makedirs(os.path.dirname(OUT_BIN), exist_ok=True)
    files = collect_files()
    blobs = []
    dir_meta = []

    for vpath, full in files:
        with open(full, "rb") as f:
            data = f.read()
        crc = zlib.crc32(data) & 0xFFFFFFFF
        vbytes = vpath.encode("utf-8")
        dir_meta.append((vbytes, len(data), crc))
        blobs.append(data)

    # Compute header + directory size
    header_size = 16
    dir_size = sum(2 + len(vb) + 12 for vb, _, _ in dir_meta)
    cur_off = header_size + dir_size

    dir_buf = bytearray()
    payload_buf = bytearray()
    for (vb, sz, crc), data in zip(dir_meta, blobs):
        dir_buf += struct.pack("<H", len(vb)) + vb + struct.pack("<III", cur_off, sz, crc)
        payload_buf += data
        cur_off += sz

    total_crc = zlib.crc32(dir_buf + payload_buf) & 0xFFFFFFFF
    header = struct.pack("<4sIII", b"VRFs", 0x00060500, len(files), total_crc)

    with open(OUT_BIN, "wb") as out:
        out.write(header)
        out.write(dir_buf)
        out.write(payload_buf)

    print(f"[VROMFS] Packed {len(files)} Dagor files ({len(header)+len(dir_buf)+len(payload_buf)} bytes) -> {OUT_BIN}")

if __name__ == "__main__":
    build_vromfs()
