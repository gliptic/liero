#!/usr/bin/env python3
"""Regenerate modern_test.lev with a Stage 4 animated ramp extension.

Adds one ramp (shift=1, 4 water-blue colours) and marks a full-width
horizontal band at the top of the level as animated.  Each pixel in the
band gets a diagonal phase offset (x+y) % ramp_color_count so adjacent
pixels shimmer out of phase.  The rest of the level is unchanged.

Run from the repository root:
    python3 tools/gen_stage4_anim.py
"""

import struct
import sys
from pathlib import Path

LEV_PATH = Path("data/TC/openliero/Levels/modern_test.lev")

W, H = 504, 350
CELLS = W * H
MAGIC = b"MODERNLV"

# Stage 4 ramp: shift=1 (advances every 2 cycles), four water-blue shades.
# Must match the constants in test_level_display.cpp.
RAMP_SHIFT = 1
RAMP_COLORS = [0xFF1A3A6A, 0xFF2A4A7A, 0xFF3A5A8A, 0xFF0A2A5A]

# Animated band: full width, this many rows tall from the top.
# Must match kBandH in the test_level_display.cpp file-based test.
BAND_HEIGHT = 20


def main():
    raw = LEV_PATH.read_bytes()

    # Locate the MODERNLV magic.
    magic_off = raw.find(MAGIC)
    if magic_off < 0:
        sys.exit("MODERNLV magic not found in modern_test.lev — is this a Stage 3 file?")

    display_data_off = magic_off + 8
    display_valid_off = display_data_off + CELLS * 4
    stage4_off = display_valid_off + CELLS

    expected_stage3_size = stage4_off
    if len(raw) < expected_stage3_size:
        sys.exit(f"File too short: {len(raw)} < {expected_stage3_size}")

    print(f"  magic at offset {magic_off}")
    print(f"  display_data at {display_data_off} ({CELLS*4} bytes)")
    print(f"  display_valid at {display_valid_off} ({CELLS} bytes)")
    print(f"  Stage 4 extension will start at {stage4_off}")
    print(f"  Animating {BAND_HEIGHT} rows × {W} cols = {BAND_HEIGHT * W} pixels")

    # Mutate display_data and display_valid in-place for the animated band.
    buf = bytearray(raw[:stage4_off])

    n_colors = len(RAMP_COLORS)
    for y in range(BAND_HEIGHT):
        for x in range(W):
            pix_idx = y * W + x
            phase_offset = (x + y) % n_colors  # diagonal shimmer
            # display_data[pix_idx] = phase_offset as uint32_t LE
            off = display_data_off + pix_idx * 4
            struct.pack_into("<I", buf, off, phase_offset)
            # display_valid[pix_idx] = 1
            buf[display_valid_off + pix_idx] = 1

    # Build Stage 4 extension: ramp_count(1) + ramp + display_anim(CELLS).
    ext = bytearray()
    ext.append(1)  # ramp_count = 1

    # Ramp entry: shift(1) + color_count(2LE) + colors(N*4).
    ext.append(RAMP_SHIFT)
    ext += struct.pack("<H", len(RAMP_COLORS))
    for c in RAMP_COLORS:
        ext += struct.pack("<I", c)

    # display_anim: 1 for band pixels, 0 for the rest.
    danim = bytearray(CELLS)
    for y in range(BAND_HEIGHT):
        for x in range(W):
            danim[y * W + x] = 1
    ext += danim

    buf += ext

    LEV_PATH.write_bytes(bytes(buf))
    print(f"Written {len(buf)} bytes ({len(buf) - stage4_off} bytes of Stage 4 extension).")
    print("Done. Run test_level_display '[file]' to verify.")


if __name__ == "__main__":
    main()
