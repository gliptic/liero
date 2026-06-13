# Level Authoring

This guide explains how to create custom terrain levels for OpenLiero — from a
simple classic level painted in [Krita](https://krita.org) to a true-color
animated level using the modern display layer.

## Overview

A level file (`.lev`) has three optional layers, each building on the previous:

| Tier | What it adds | Rendered in |
|------|-------------|-------------|
| **Classic** | Material map only (palette-indexed) | All modes |
| **POWERLEVEL** | Custom 256-color palette | All modes |
| **Modern** (`MODERNLV`) | True-color and/or animated display | Modern mode only |

The **material map** is always present and is the simulation source of truth:
it controls collision, destructibility, shadows, and the minimap. The display
layers only affect what the player sees. Classic-mode renderers (including
older replays and netplay peers without the modern extension) ignore the
`MODERNLV` block entirely and render from the palette as usual.

---

## 1. Classic Level Authoring

### Canvas size

Every level is exactly **504 × 350 pixels**. The game does not resize or crop.

### Material indices

Each pixel in the material map is a **material index** — a number from 0 to 255
that determines how that pixel behaves in simulation. Flags in the TC's material
table control physics; the palette controls what color each index displays.

In the **openliero TC** the usable index ranges are:

| Index range | Flags | Physics |
|-------------|-------|---------|
| 130, 160–167 | Background (±SeeShadow) | **Open space** — worms move through freely; shots pass through |
| 1–2, 77–79 | Dirt + Background | Passable by worms; shots destroy the pixel |
| 12–18, 55–58, 82–90, 94–103, 120–122, 176–180 | Dirt (variants) | **Solid** to worms; shots destroy the pixel |
| 19–29, 59–61, 85–87, 91–93, 123–125 | Rock | **Solid** to worms; shots hit and bounce |
| 30–38 | WormM | **Solid** to worms; shots pass through — yellow barrier in the default palette |
| 168–175 | (none) | Solid to worms; shots pass through; palette-animated display — water shimmer effect |

Any index not in the table above (0, 3–11, 39–54, 62–76, 80–81, 104–119,
126–129, 131–159, 181–255) has no flags: solid to worms, transparent to shots,
no assigned palette color. These are effectively unused slots.

**For a minimal playable level** you need three things: index 160 (open space),
index 12 (dirt), and index 19 (rock). Index 160 is what worms actually move
through — it has the Background flag that makes pixels passable.

> **Index 0 is solid**, not air. It has no flags at all, which makes it solid
> to worms and transparent to shots (same physics as the yellow-barrier range).
> Use index 160 for open space.

> **Other TCs**: material indices and their behaviors are defined per-TC in
> `tc.cfg`. If you are authoring a level for a TC other than openliero, check
> that TC's material table.

### Krita workflow

Krita works best for this in **RGBA mode** with a fixed set of key colors. The
Python script converts those key colors to material indices.

Suggested palette for `material.png`:

| Color | Hex | Material index |
|-------|-----|---------------|
| White | `#FFFFFF` | 160 — open space (Background; worms move through it) |
| Saddle brown | `#8B4513` | 12 — dirt (solid, destroyed by weapons) |
| Slate grey | `#708090` | 19 — rock (indestructible) |
| Yellow | `#FFFF00` | 30 — worm barrier (solid to worms, shots pass through) |
| Dark navy | `#1A3A6A` | 168 — water shimmer (solid, palette-animated) |

1. Open Krita and create a new document: **504 × 350 px** at any resolution.
2. In **Brush Settings**, set **Anti-aliasing** to off and brush opacity to
   100 %. Anti-aliased edges produce blended colors that the conversion script
   cannot map to a material index; those pixels are treated as open space
   (index 160).
3. Flood-fill the entire canvas **white** first — that sets every pixel to
   open space. Then paint terrain on top.
4. **File → Export As** → `material.png` (PNG format, RGBA, any bit depth).

Alternatively, use Krita's indexed color mode:

- **Image → Convert Image Color Space → Indexed**
- Assign your 256-color palette so that slot N maps to material index N.
- Export as an 8-bpp indexed PNG — the script reads palette indices directly
  and no color lookup is needed.

### Generating the `.lev` file

Run `tools/lev_gen.py` (see [section 6](#6-toolslev_genpy)):

```bash
uv run tools/lev_gen.py --mat material.png --out mymap.lev
```

---

## 2. POWERLEVEL: Custom Palette

The POWERLEVEL extension lets you ship a custom 256-color palette with your
level. That palette replaces the TC's default one when the level loads,
changing the color of every palette-derived pixel in both classic and modern
modes.

**Palette encoding**: each of the 256 entries is stored as three bytes (R, G,
B) in 6-bit VGA format, values 0–63. To convert an 8-bit channel to 6-bit:
`v6 = v8 >> 2`. The loader reverses this: `v8_display = (v6 & 63) << 2`.

Supply an indexed-color PNG whose embedded palette becomes the POWERLEVEL block:

```bash
uv run tools/lev_gen.py --mat material.png --pal palette.png --out mymap.lev
```

`palette.png` must be an 8-bpp indexed PNG. You can create one in Krita by
converting a 1 × 256 px swatch image to indexed color mode and exporting it.

The POWERLEVEL block is written **before** the MODERNLV block in the file. The
script handles the ordering automatically.

---

## 3. Modern Display Layer

The `MODERNLV` block adds a **display layer** — a parallel ARGB32 array of the
same 504 × 350 dimensions. In modern mode, any pixel where `display_valid == 1`
is rendered using the authored color from `display_data` instead of the palette
lookup.

### Key rules

- The material map is still required and still drives all gameplay. You can
  author any display color over any material index without changing how the
  terrain behaves.
- When terrain is destroyed, `display_valid` is cleared for that pixel. The
  authored color is gone until the level reloads — blast craters always fall
  back to the palette.
- Pixels left with `display_valid == 0` behave exactly as in classic mode,
  including palette animation for water.

### Krita workflow

You need two PNG files: the `material.png` from section 1, and a new
`display.png` for what the terrain looks like.

1. Create a new **504 × 350 px** document in Krita: `display.png`.
2. Paint your terrain in full color, layer by layer.
3. Where you want the **palette** to control the color (e.g. palette-animated
   water, or any area you haven't painted), set those pixels to fully
   transparent (`alpha = 0`).
4. Where you want the **exact color you painted** to show in modern mode, paint
   with `alpha = 255`.
5. **File → Export As** → `display.png` (PNG, RGBA).

Generate the level:

```bash
uv run tools/lev_gen.py --mat material.png --disp display.png --out mymap.lev
```

### Example: static rock over palette-animated water

True-color rock sitting above solid animated water, with open sky above that:

| Pixel type | material.png color | display.png alpha | Result |
|---|---|---|---|
| Open sky | `#FFFFFF` (index 160) | 0 — transparent | Both modes: empty passable space |
| Rock | `#708090` (index 19) | 255 — painted grey | Modern: your color; classic: palette |
| Water | `#1A3A6A` (index 168) | 0 — transparent | Both modes: solid, palette-animated shimmer |

Index 168 is **solid to worms** (they stand on top of it, not swim through it).
The palette animation makes it look like shimmering water, but physically it
behaves like the yellow barrier range — worms block, shots pass through.

---

## 4. Animated Terrain

For water and other cycling effects you have two options:

**Option A — Leave pixels transparent in `display.png`**: the pixel uses the
palette as normal. The openliero TC's `colorAnim` configuration animates
indices 168–171 (water shimmer) and several others. Leaving water pixels
transparent in `display.png` is the recommended approach — it costs nothing and
the palette-cycle effect is already there.

**Option B — Use a MODERNLV animation ramp**: define your own color sequence
independent of the palette. Useful for custom effects like glowing lava or
neon water that don't match any palette range.

### How ramps work

An animation ramp is a short list of ARGB32 colors. Each frame, the renderer
advances through the list at a rate controlled by `shift`:

```
phase  = (display_data[pixel] + (game_cycles >> shift)) % color_count
color  = ramp.colors[phase]
```

- `shift = 0` — advances every frame (fast flicker)
- `shift = 3` — advances every 8 frames (gentle shimmer)
- `display_data[pixel]` stores a **per-pixel phase offset** (0 to
  `color_count − 1`) so that adjacent pixels don't all change color in unison,
  producing a wave-like effect.

### Defining ramps (ramps.json)

Create a JSON file listing your ramps. The first entry in the array is ramp 1
in the level; index 0 means "no animation."

```json
[
  {
    "shift": 3,
    "colors": ["#1A3A6A", "#2A4A7A", "#1A3A6A", "#0A2A5A"]
  },
  {
    "shift": 1,
    "colors": ["#FF4400", "#FF6600", "#FF8800", "#FF6600"]
  }
]
```

Up to 255 ramps, up to 4096 colors each.

### Krita workflow for animated terrain

Create an **anim map** PNG (`anim.png`, 504 × 350, RGBA) with this encoding
per pixel:

| Channel | Value | Meaning |
|---------|-------|---------|
| Alpha | 0 | Not animated (handled by display.png or palette) |
| Alpha | 255 | Animated pixel |
| Red | 1–255 | Ramp index (1-based; must be ≤ ramp count in ramps.json) |
| Green | 0–255 | Phase offset (staggers adjacent pixels) |
| Blue | — | Unused |

Tips for authoring in Krita:

- For the **phase offset** (green channel), paint a smooth gradient across
  animated regions using a color with R=1, G varying from 0 upward. Neighboring
  pixels with different G values produce a flowing wave.
- Leave static `display.png` areas intact — animated pixels from `anim.png`
  override those positions.

Generate the level:

```bash
uv run tools/lev_gen.py --mat material.png --disp display.png \
    --ramps ramps.json --anim anim.png --out mymap.lev
```

### Worked example: 4-color water shimmer

```json
[{ "shift": 3, "colors": ["#1A3A6A", "#2A4A7A", "#1A3A6A", "#0A2A5A"] }]
```

In `anim.png`, paint water pixels with R=1 (ramp 1). Vary G from 0 to 3
across adjacent pixels. At game frame 40 (`cycles = 40`):

```
pixel A  G=0:  phase = (0 + (40 >> 3)) % 4 = (0 + 5) % 4 = 1 -> #2A4A7A
pixel B  G=1:  phase = (1 + 5) % 4          = 2             -> #1A3A6A
pixel C  G=2:  phase = (2 + 5) % 4          = 3             -> #0A2A5A
pixel D  G=3:  phase = (3 + 5) % 4          = 0             -> #1A3A6A
```

Each pixel is one step behind its neighbor, producing a moving shimmer.

---

## 5. Placing Your Level

The in-game level picker is a file browser rooted at the **user config
directory** (or the portable directory). You can place your `.lev` file
anywhere inside that root — directly at the top level or in any subfolder you
create — and it will appear in the picker when you navigate to it.

**Portable install** (the `.tar.gz` or `.zip` archive): place the file
anywhere inside the extracted folder, for example:

```
mymap.lev
```

**Per-user install**: place the file inside the user config directory:

| Platform | User config directory |
|----------|-----------------------|
| Linux | `~/.local/share/openliero/openliero/` |
| macOS | `~/Library/Application Support/openliero/openliero/` |
| Windows | `%APPDATA%\openliero\openliero\` |

You can organise levels into subfolders (e.g. `Levels/mymap.lev`) — the picker
lets you navigate into them.

To select your level in-game: open the **Settings** menu, navigate to
**Level**, and browse to your file. The level is loaded when a new game starts.

---

## 6. `tools/lev_gen.py`

Requires [uv](https://docs.astral.sh/uv/). All command forms:

```bash
# Classic
uv run tools/lev_gen.py --mat material.png --out level.lev

# + custom palette
uv run tools/lev_gen.py --mat material.png --pal palette.png --out level.lev

# Modern display layer
uv run tools/lev_gen.py --mat material.png --disp display.png --out level.lev

# Modern + animation
uv run tools/lev_gen.py --mat material.png --disp display.png \
    --ramps ramps.json --anim anim.png --out level.lev
```

---

## 7. `tools/lev_extract.py`

The companion script to `lev_gen.py`. Given any `.lev` file it writes back the
constituent PNGs and JSON that `lev_gen.py` accepts, making it easy to inspect
or modify an existing level.

```bash
uv run tools/lev_extract.py level.lev --out-dir krita/mymap/
```

Outputs (only those present in the file are written):

| File | Contents |
|------|----------|
| `material.png` | RGB material map using the same key colours as `lev_gen.py` |
| `display.png` | RGBA display layer — alpha 255 = authored colour, alpha 0 = palette |
| `palette.png` | 1×256 indexed swatch for POWERLEVEL levels |
| `ramps.json` | Animation ramp definitions |
| `anim.png` | RGBA — R = ramp index, G = phase offset, alpha 255 where animated |

### Working example: deconstructing `modern_test.lev`

`data/TC/openliero/Levels/modern_test.lev` ships with the repository and
exercises all three layers (material map, MODERNLV display, animation ramp).
It is a good starting point for building your own level.

```bash
uv run tools/lev_extract.py data/TC/openliero/Levels/modern_test.lev \
    --out-dir krita/modern_test/
```

This produces four files in `krita/modern_test/`:

- **`material.png`** — the 504×350 material map. White pixels are open space
  (index 160), brown is dirt (12), dark navy is the water band (168).
- **`display.png`** — true-color display pixels with transparency where the
  palette takes over. Open the material and display PNGs as layers in Krita;
  paint on `display.png`, keep the terrain structure in `material.png`.
- **`ramps.json`** — one ramp entry: four water-blue shades at `shift=1`.
- **`anim.png`** — R=1 (ramp 1) for the animated band; G encodes the
  diagonal phase offset that produces the shimmer.

Edit any of these in Krita, then re-encode:

```bash
uv run tools/lev_gen.py \
    --mat  krita/modern_test/material.png \
    --disp krita/modern_test/display.png \
    --ramps krita/modern_test/ramps.json \
    --anim  krita/modern_test/anim.png \
    --out   mymap.lev
```

---

## Appendix: File Format Specification

### File layout

```
[material_id : 504 x 350 bytes]         <- always present (palette indices 0-255)
["POWERLEVEL" + palette : 778 bytes]    <- optional; must appear before MODERNLV
["MODERNLV" + display block + anim]     <- optional; must appear at end of file
```

### MODERNLV block layout

| Field | Size (bytes) | Description |
|-------|-------------|-------------|
| magic | 8 | ASCII `MODERNLV` |
| display_data | 504 x 350 x 4 | ARGB32 little-endian (`0xAARRGGBB`), alpha always `0xFF` |
| display_valid | 504 x 350 | 1 = authored pixel, 0 = palette fallback |
| ramp_count | 1 | Number of animation ramps (0 = no animation layer) |
| ramp table | variable | Present only when ramp_count > 0 |
| display_anim | 504 x 350 | Per-pixel ramp index; present only when ramp_count > 0 |

### Ramp table (one entry per ramp)

| Field | Size (bytes) | Description |
|-------|-------------|-------------|
| shift | 1 | Right-shift applied to `cycles` before indexing |
| color_count | 2 (LE) | Number of ARGB32 colors in this ramp (1-4096) |
| colors | color_count x 4 | ARGB32 entries, little-endian |

### Pixel semantics

**Static authored pixel** (`display_valid = 1`, `display_anim = 0`): the
renderer uses `display_data[idx]` as the ARGB color.

**Animated pixel** (`display_valid = 1`, `display_anim = N`, N >= 1):
`display_data[idx]` is a phase offset (not a color). The resolved color is:

```
phase  = (display_data[idx] + (cycles >> ramp.shift)) % ramp.color_count
colour = ramp.colors[phase]
```

**Palette-derived pixel** (`display_valid = 0`): the renderer falls back to
`pal32[material_id[idx]]` in both classic and modern modes.

### POWERLEVEL palette encoding

The POWERLEVEL block is 10 bytes of magic (`POWERLEVEL`) followed by 768 bytes
of palette data (256 entries x 3 bytes). Each byte is a 6-bit VGA channel
(0-63). Loader conversion: `display_value = (file_byte & 63) << 2`. Script
conversion: `file_byte = channel_8bit >> 2`.

### Compatibility

- Levels without the `MODERNLV` block load in all versions with no visual
  difference from a classic level.
- Files where the stream ends after `display_valid` are treated as
  `ramp_count = 0` (no animation layer).
- The MODERNLV block including the animation extension is forwarded over
  netplay and embedded in replays. Peers or replays without animation support
  load with an empty animation layer (static display only).
- In classic mode all display and animation data is ignored.
