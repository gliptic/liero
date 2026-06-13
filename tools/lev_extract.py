#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.8"
# dependencies = [
#   "Pillow>=10.0",
# ]
# ///
"""lev_extract.py — Deconstruct a .lev file into its constituent parts.

Writes into --out-dir (default: current directory):
  material.png  — material map (colours match lev_gen.py key colours for round-trips)
  display.png   — MODERNLV display layer (alpha=255 → authored colour, alpha=0 → palette)
  palette.png   — POWERLEVEL palette swatch (1×256 indexed PNG)
  ramps.json    — animation ramp definitions
  anim.png      — animation map (R=ramp index 1-based, G=phase offset, alpha=255 where animated)

Only files present in the level are written.

Usage:
  uv run tools/lev_extract.py level.lev
  uv run tools/lev_extract.py level.lev --out-dir krita/mymap/

See docs/modern-level-authoring.md for the full authoring guide.
"""

import argparse
import json
import struct
import sys
from pathlib import Path
from PIL import Image

LEVEL_W, LEVEL_H = 504, 350
CELLS = LEVEL_W * LEVEL_H


def err(msg: str) -> None:
    sys.exit(f"ERROR: {msg}")


# Per-index colours that round-trip cleanly through lev_gen.py's COLOUR_MAP.
_EXACT_COLOUR: dict[int, tuple[int, int, int]] = {
    0:   ( 64,  64,  64), # index 0: solid, shot-passable (dark grey)
    12:  (139,  69,  19), # dirt
    13:  (108,  54,  15), # dirt variant
    19:  (112, 128, 144), # rock
    30:  (255, 255,   0), # worm barrier (yellow)
    168: ( 26,  58, 106), # water shimmer (dark navy)
}


def _index_colour(idx: int) -> tuple[int, int, int]:
    """Return an RGB colour representing material index idx in material.png."""
    if idx in _EXACT_COLOUR:
        return _EXACT_COLOUR[idx]
    # Background-flagged = open space -> white (round-trips as index 160 via lev_gen.py)
    if idx in (1, 2, 77, 78, 79, 130) or 160 <= idx <= 167:
        return (255, 255, 255)
    # Dirt variants (not exact-mapped) -> saddle brown
    if (14 <= idx <= 18 or 55 <= idx <= 58 or 82 <= idx <= 90
            or 94 <= idx <= 103 or 120 <= idx <= 122 or 176 <= idx <= 180):
        return (139, 69, 19)
    # Rock variants -> slate grey
    if (20 <= idx <= 29 or 59 <= idx <= 61 or 85 <= idx <= 87
            or 91 <= idx <= 93 or 123 <= idx <= 125):
        return (112, 128, 144)
    # WormM variants -> yellow
    if 31 <= idx <= 38:
        return (255, 255, 0)
    # Water shimmer variants -> dark navy
    if 169 <= idx <= 175:
        return (26, 58, 106)
    # Unlisted (solid, shot-passable, no assigned palette colour) -> magenta
    return (200, 0, 200)


def write_material(mat: bytes, path: Path) -> None:
    img = Image.new("RGB", (LEVEL_W, LEVEL_H))
    img.putdata([_index_colour(b) for b in mat])
    img.save(path)
    print(f"  material.png   — {LEVEL_W}×{LEVEL_H} material map")


def write_display(dd: bytes, dv: bytes, path: Path) -> None:
    pixels: list[tuple[int, int, int, int]] = []
    for i in range(CELLS):
        if dv[i]:
            argb = struct.unpack_from("<I", dd, i * 4)[0]
            r = (argb >> 16) & 0xFF
            g = (argb >> 8) & 0xFF
            b = argb & 0xFF
            pixels.append((r, g, b, 255))
        else:
            pixels.append((0, 0, 0, 0))
    img = Image.new("RGBA", (LEVEL_W, LEVEL_H))
    img.putdata(pixels)
    img.save(path)
    authored = sum(1 for v in dv if v)
    print(f"  display.png    — {authored:,} authored pixels, rest transparent")


def write_palette(raw: bytes, path: Path) -> None:
    # raw is 256×3 bytes of 6-bit VGA (0–63); expand to 8-bit for PIL.
    flat: list[int] = []
    for i in range(256):
        flat.append((raw[i * 3]     & 63) << 2)
        flat.append((raw[i * 3 + 1] & 63) << 2)
        flat.append((raw[i * 3 + 2] & 63) << 2)
    img = Image.new("P", (1, 256))
    img.putpalette(flat)
    img.putdata(list(range(256)))
    img.save(path)
    print(f"  palette.png    — 1×256 indexed swatch")


def write_anim(ramps: list[dict], da: bytes, dd: bytes,
               ramps_path: Path, anim_path: Path) -> None:
    with open(ramps_path, "w") as f:
        json.dump(ramps, f, indent=2)
    print(f"  ramps.json     — {len(ramps)} ramp(s)")

    pixels: list[tuple[int, int, int, int]] = []
    for i in range(CELLS):
        r_idx = da[i]
        if r_idx:
            # Phase offset was packed as uint32 LE in display_data.
            phase = struct.unpack_from("<I", dd, i * 4)[0] & 0xFF
            pixels.append((r_idx, phase, 0, 255))
        else:
            pixels.append((0, 0, 0, 0))
    img = Image.new("RGBA", (LEVEL_W, LEVEL_H))
    img.putdata(pixels)
    img.save(anim_path)
    animated = sum(1 for b in da if b)
    print(f"  anim.png       — {animated:,} animated pixels (R=ramp, G=phase)")


def main() -> None:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("lev", metavar="LEV", help=".lev file to extract")
    ap.add_argument("--out-dir", metavar="DIR", default=".",
                    help="output directory (default: current directory)")
    args = ap.parse_args()

    lev_path = Path(args.lev)
    if not lev_path.exists():
        err(f"file not found: {lev_path}")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    data = lev_path.read_bytes()
    if len(data) < CELLS:
        err(f"file too small: {len(data)} bytes (need at least {CELLS})")

    mat = data[:CELLS]
    rest = data[CELLS:]
    pos = 0

    raw_palette: bytes | None = None
    dd: bytes | None = None
    dv: bytes | None = None
    ramps: list[dict] = []
    da: bytes | None = None

    while pos < len(rest):
        if rest[pos:pos + 10] == b"POWERLEVEL":
            pos += 10
            if pos + 768 > len(rest):
                print("WARNING: POWERLEVEL block truncated, skipping")
                break
            raw_palette = rest[pos:pos + 768]
            pos += 768

        elif rest[pos:pos + 8] == b"MODERNLV":
            pos += 8
            if pos + CELLS * 5 + 1 > len(rest):
                print("WARNING: MODERNLV block truncated, skipping")
                break
            dd = rest[pos:pos + CELLS * 4]
            dv = rest[pos + CELLS * 4:pos + CELLS * 5]
            ramp_count = rest[pos + CELLS * 5]
            pos += CELLS * 5 + 1

            for ri in range(ramp_count):
                if pos + 3 > len(rest):
                    print(f"WARNING: ramp {ri} header truncated")
                    break
                shift = rest[pos]
                color_count = struct.unpack_from("<H", rest, pos + 1)[0]
                pos += 3
                if pos + color_count * 4 > len(rest):
                    print(f"WARNING: ramp {ri} colours truncated")
                    break
                colours = []
                for ci in range(color_count):
                    argb = struct.unpack_from("<I", rest, pos + ci * 4)[0]
                    r8 = (argb >> 16) & 0xFF
                    g8 = (argb >> 8)  & 0xFF
                    b8 = argb         & 0xFF
                    colours.append(f"#{r8:02X}{g8:02X}{b8:02X}")
                pos += color_count * 4
                ramps.append({"shift": int(shift), "colors": colours})

            if ramp_count > 0:
                if pos + CELLS > len(rest):
                    print("WARNING: display_anim truncated")
                else:
                    da = rest[pos:pos + CELLS]
                    pos += CELLS
        else:
            break  # unknown or no more blocks

    print(f"Extracting {lev_path.name}:")
    write_material(mat, out_dir / "material.png")
    if raw_palette is not None:
        write_palette(raw_palette, out_dir / "palette.png")
    if dd is not None and dv is not None:
        write_display(dd, dv, out_dir / "display.png")
        if ramps and da is not None:
            write_anim(ramps, da, dd, out_dir / "ramps.json", out_dir / "anim.png")
    print(f"Done. Output in {out_dir}/")


if __name__ == "__main__":
    main()
