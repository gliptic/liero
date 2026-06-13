#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.8"
# dependencies = [
#   "Pillow>=10.0",
# ]
# ///
"""lev_gen.py — Build a .lev file for OpenLiero from Krita-exported PNGs.

Usage examples:
  Classic:   uv run tools/lev_gen.py --mat material.png --out level.lev
  +palette:  uv run tools/lev_gen.py --mat material.png --pal palette.png --out level.lev
  Modern:    uv run tools/lev_gen.py --mat material.png --disp display.png --out level.lev
  Animated:  uv run tools/lev_gen.py --mat material.png --disp display.png \\
                                     --ramps ramps.json --anim anim.png --out level.lev

See docs/modern-level-authoring.md for a full authoring guide.
"""

import argparse
import json
import struct
import sys
from PIL import Image

LEVEL_W, LEVEL_H = 504, 350
CELLS = LEVEL_W * LEVEL_H

# Key-color -> material-index map (openliero TC defaults).
# Edit this table to match your TC or add colors you painted with.
# Index 160 (Background+SeeShadow) is open space — worms move through it.
# Index 0 has NO Background flag and is solid to worms; don't use it for sky.
COLOUR_MAP: dict[tuple[int, int, int], int] = {
    (255, 255, 255): 160,  # white        -> open space (Background; worms pass through)
    (139, 69,  19 ): 12,   # saddle brown -> dirt (solid, destroyed by weapons)
    (108, 54,  15 ): 13,   # dark brown   -> dirt variant
    (112, 128, 144): 19,   # slate grey   -> rock (indestructible)
    (255, 255,  0  ): 30,  # yellow       -> worm barrier (solid to worms, shots pass through)
    (26,  58,  106): 168,  # dark navy    -> water shimmer (solid, palette-animated)
    (64,  64,  64  ): 0,   # dark grey    -> index 0 (solid, shot-passable; used by lev_extract)
}


def err(msg: str) -> None:
    sys.exit(f"ERROR: {msg}")


def load_mat(path: str) -> bytes:
    img = Image.open(path)
    if img.size != (LEVEL_W, LEVEL_H):
        err(f"{path}: must be {LEVEL_W}x{LEVEL_H} px, got {img.size}")
    if img.mode == "P":
        # Indexed PNG: palette slot number = material index directly.
        return img.tobytes()
    # RGBA/RGB: map key colors to material indices.
    rgba = img.convert("RGBA")
    raw = rgba.tobytes()
    out = bytearray([160] * CELLS)  # default to open space (Background-flagged)
    unknown: dict[tuple[int, int, int], int] = {}
    for i in range(CELLS):
        r, g, b = raw[i * 4], raw[i * 4 + 1], raw[i * 4 + 2]
        mat = COLOUR_MAP.get((r, g, b))
        if mat is None:
            unknown[(r, g, b)] = unknown.get((r, g, b), 0) + 1
        else:
            out[i] = mat
    if unknown:
        print("WARNING: unrecognised colors treated as open space (index 160):")
        for rgb, n in sorted(unknown.items(), key=lambda kv: -kv[1])[:10]:
            print(f"  rgb{rgb} x {n}")
    return bytes(out)


def load_pal(path: str) -> bytes:
    """Return 768 bytes: 256 x (R6, G6, B6) in 6-bit VGA format."""
    img = Image.open(path)
    if img.mode != "P":
        err(f"{path}: must be an indexed-palette PNG for --pal")
    raw = img.getpalette()  # [R0,G0,B0, R1,G1,B1, ...] in 8-bit
    out = bytearray(768)
    for i in range(256):
        out[i * 3]     = (raw[i * 3]     >> 2) & 0x3F
        out[i * 3 + 1] = (raw[i * 3 + 1] >> 2) & 0x3F
        out[i * 3 + 2] = (raw[i * 3 + 2] >> 2) & 0x3F
    return bytes(out)


def argb32(r: int, g: int, b: int) -> int:
    """Pack RGB into ARGB32 (alpha=0xFF) as a little-endian uint32."""
    return (0xFF << 24) | (r << 16) | (g << 8) | b


def load_disp(path: str) -> tuple[bytearray, bytearray]:
    """Return (display_data, display_valid).

    Opaque pixels (alpha > 0) are authored; transparent pixels fall back to
    the palette. display_data stores ARGB32 values, little-endian.
    """
    img = Image.open(path).convert("RGBA")
    if img.size != (LEVEL_W, LEVEL_H):
        err(f"{path}: must be {LEVEL_W}x{LEVEL_H} px")
    raw = img.tobytes()
    dd = bytearray(CELLS * 4)
    dv = bytearray(CELLS)
    for i in range(CELLS):
        r, g, b, a = raw[i * 4], raw[i * 4 + 1], raw[i * 4 + 2], raw[i * 4 + 3]
        if a > 0:
            struct.pack_into("<I", dd, i * 4, argb32(r, g, b))
            dv[i] = 1
    return dd, dv


def main() -> None:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--mat",   required=True, metavar="PNG",  help="material map")
    ap.add_argument("--disp",  metavar="PNG",  help="display layer (RGBA)")
    ap.add_argument("--pal",   metavar="PNG",  help="indexed PNG -> POWERLEVEL palette")
    ap.add_argument("--ramps", metavar="JSON", help="animation ramp definitions")
    ap.add_argument("--anim",  metavar="PNG",  help="animation map (R=ramp, G=phase)")
    ap.add_argument("--out",   required=True,  metavar="LEV",  help="output .lev file")
    args = ap.parse_args()

    if args.anim and not args.ramps:
        err("--anim requires --ramps")
    if args.anim and not args.disp:
        err("--anim requires --disp")

    mat = load_mat(args.mat)

    dd = bytearray(CELLS * 4)
    dv = bytearray(CELLS)
    da = bytearray(CELLS)   # display_anim: 0=no anim, N=ramp N (1-based)
    ramps: list[dict] = []

    if args.disp:
        dd, dv = load_disp(args.disp)

    if args.ramps:
        with open(args.ramps) as f:
            ramps = json.load(f)
        if not (1 <= len(ramps) <= 255):
            err("ramps.json: need 1-255 ramps")
        for idx, ramp in enumerate(ramps):
            if "shift" not in ramp or "colors" not in ramp:
                err(f"ramp {idx}: must have 'shift' and 'colors'")
            if not (1 <= len(ramp["colors"]) <= 4096):
                err(f"ramp {idx}: 1-4096 colors required")

    if args.anim:
        anim_img = Image.open(args.anim).convert("RGBA")
        if anim_img.size != (LEVEL_W, LEVEL_H):
            err(f"{args.anim}: must be {LEVEL_W}x{LEVEL_H} px")
        anim_raw = anim_img.tobytes()
        for i in range(CELLS):
            r, g, b, a = anim_raw[i * 4], anim_raw[i * 4 + 1], anim_raw[i * 4 + 2], anim_raw[i * 4 + 3]
            if a > 0 and r > 0:
                if r > len(ramps):
                    x, y = i % LEVEL_W, i // LEVEL_W
                    err(f"anim.png pixel ({x},{y}): ramp index {r} "
                        f"exceeds ramp count {len(ramps)}")
                dv[i] = 1                              # must be valid for anim pixels
                da[i] = r                              # ramp index (1-based)
                struct.pack_into("<I", dd, i * 4, g)   # phase offset replaces color

    with open(args.out, "wb") as f:
        f.write(mat)

        if args.pal:
            f.write(b"POWERLEVEL")
            f.write(load_pal(args.pal))

        if args.disp:
            f.write(b"MODERNLV")
            f.write(dd)
            f.write(dv)

            if ramps:
                f.write(bytes([len(ramps)]))
                for ramp in ramps:
                    colors_bytes = bytearray()
                    for hx in ramp["colors"]:
                        hx = hx.lstrip("#")
                        r8 = int(hx[0:2], 16)
                        g8 = int(hx[2:4], 16)
                        b8 = int(hx[4:6], 16)
                        colors_bytes += struct.pack("<I", argb32(r8, g8, b8))
                    f.write(bytes([ramp["shift"]]))
                    f.write(struct.pack("<H", len(ramp["colors"])))
                    f.write(colors_bytes)
                f.write(da)
            else:
                f.write(b"\x00")  # ramp_count = 0

    parts = ["material map"]
    if args.pal:   parts.append("POWERLEVEL palette")
    if args.disp:  parts.append("MODERNLV display layer")
    if ramps:      parts.append(f"{len(ramps)} animation ramp(s)")
    print(f"Written {args.out}: {', '.join(parts)}")


if __name__ == "__main__":
    main()
