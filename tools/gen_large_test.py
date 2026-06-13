#!/usr/bin/env python3
"""gen_large_test.py — Generate a 4096×4096 OLLEVEL2 test level on demand.

The file is NOT committed to git (~268 MB uncompressed) and is only generated
when you need to exercise the large-map code path (PR 2 + PR 4 validation).

Output: data/TC/openliero/Levels/large_test.lev

Layout:
  - Open sky (index 160) fills the top half.
  - Solid rock border (index 19) lines the edges.
  - Dirt fill (index 12) in the lower half.
  - Animated MODERNLV band: 20 rows at the top of the dirt area use ramp 1
    (4-color water shimmer), matching the modern_test.lev convention.

Run from the repository root:
    python3 tools/gen_large_test.py
"""

import struct
import sys
from pathlib import Path

W, H = 4096, 4096
CELLS = W * H

OUT_PATH = Path("data/TC/openliero/Levels/large_test.lev")

SIZED_MAGIC = b"OLLEVEL2"
MODERNLV_MAGIC = b"MODERNLV"

# Material indices (openliero TC).
MAT_SKY = 160   # Background+SeeShadow: passable
MAT_ROCK = 19   # Rock: indestructible
MAT_DIRT = 12   # Dirt: solid, destructible

# Match modern_test.lev ramp so the same test constants apply.
RAMP_SHIFT = 1
RAMP_COLORS = [0xFF1A3A6A, 0xFF2A4A7A, 0xFF3A5A8A, 0xFF0A2A5A]
BAND_HEIGHT = 20  # animated rows at top of dirt area


def build_material() -> bytearray:
    print(f"  Building {W}×{H} material map ({CELLS // 1_000_000:.0f} M cells)…")
    mat = bytearray(CELLS)

    sky_rows = H // 2

    for y in range(H):
        for x in range(W):
            idx = y * W + x
            if x == 0 or x == W - 1 or y == 0 or y == H - 1:
                mat[idx] = MAT_ROCK      # rock border
            elif y < sky_rows:
                mat[idx] = MAT_SKY       # open sky
            else:
                mat[idx] = MAT_DIRT      # dirt lower half
    return mat


def build_modernlv(mat: bytearray) -> tuple[bytearray, bytearray, bytearray, bytearray]:
    """Return (display_data, display_valid, ramp_table_bytes, display_anim)."""
    print("  Building MODERNLV display + animation layer…")
    dd = bytearray(CELLS * 4)
    dv = bytearray(CELLS)
    da = bytearray(CELLS)

    sky_rows = H // 2
    band_start = sky_rows  # first row of the dirt area
    n_colors = len(RAMP_COLORS)

    for y in range(band_start, band_start + BAND_HEIGHT):
        for x in range(W):
            idx = y * W + x
            phase = (x + y) % n_colors
            struct.pack_into("<I", dd, idx * 4, phase)
            dv[idx] = 1
            da[idx] = 1  # ramp 1

    # Ramp table bytes.
    ramp_bytes = bytearray()
    ramp_bytes.append(1)        # ramp_count = 1
    ramp_bytes.append(RAMP_SHIFT)
    ramp_bytes += struct.pack("<H", len(RAMP_COLORS))
    for c in RAMP_COLORS:
        ramp_bytes += struct.pack("<I", c)

    return dd, dv, ramp_bytes, da


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)

    mat = build_material()
    dd, dv, ramp_bytes, da = build_modernlv(mat)

    print(f"  Writing {OUT_PATH} …")
    with open(OUT_PATH, "wb") as f:
        # OLLEVEL2 header.
        f.write(SIZED_MAGIC)
        f.write(bytes([0]))                   # version
        f.write(struct.pack("<H", W))
        f.write(struct.pack("<H", H))

        # Material data.
        f.write(mat)

        # MODERNLV block.
        f.write(MODERNLV_MAGIC)
        f.write(dd)
        f.write(dv)
        f.write(ramp_bytes)
        f.write(da)

    size_mb = OUT_PATH.stat().st_size / 1_048_576
    print(f"Done. {OUT_PATH} ({size_mb:.0f} MB)")
    print("NOTE: this file is in .gitignore — do not commit it.")


if __name__ == "__main__":
    main()
