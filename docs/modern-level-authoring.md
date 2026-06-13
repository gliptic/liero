# Modern Level Authoring (Stages 3 & 4)

This guide describes how to author true-color terrain art for OpenLiero TC
packs, including the Stage 4 animated ramp extension.

## Background

Every `.LEV` file contains 504 × 350 bytes of **material identifiers** (palette
indices 0–255). These drive collision, destructibility, shadow logic, and
minimap colour — they are the simulation source of truth and are never replaced
by Stage 3.

Stage 3 adds an optional **display layer**: a parallel ARGB32 array of the same
dimensions. When a pixel has an authored ARGB value (`display_valid == 1`),
the modern-mode renderer uses that colour directly instead of looking up the
palette. Classic-mode renderers ignore the display layer entirely; they always
use the palette index.

Existing TC packs load unchanged. The display layer is absent from every
classic level, so there is no visual difference in either mode.

## File Format

A level file is a flat byte stream:

```
[material_id : 504*350 bytes]           ← always present (palette indices)
["POWERLEVEL" + palette : 778 bytes]    ← optional (custom palette extension)
["MODERNLV" + Stage3 block + Stage4 extension]  ← optional (display + anim)
```

The `MODERNLV` block, if present, must appear at the end of the file, after
the optional `POWERLEVEL` block.

### MODERNLV block layout

| Field          | Size (bytes)      | Description                            |
|----------------|-------------------|----------------------------------------|
| magic          | 8                 | ASCII literal `MODERNLV`               |
| display_data   | 504 × 350 × 4     | ARGB32, little-endian, row-major       |
| display_valid  | 504 × 350         | 1 = authored (use display_data), 0 = palette fallback |
| ramp_count     | 1                 | Number of animation ramps (0 = no anim layer, Stage-3-only file) |
| ramp table     | variable          | Present only if ramp_count > 0 (see below) |
| display_anim   | 504 × 350         | Per-pixel ramp index (present only if ramp_count > 0) |

#### Ramp table (one entry per ramp)

| Field        | Size (bytes) | Description                                      |
|--------------|--------------|--------------------------------------------------|
| shift        | 1            | Right-shift applied to `cycles` before indexing  |
| color_count  | 2 (LE)       | Number of ARGB32 colours in this ramp (1–4096)   |
| colors       | color_count × 4 | ARGB32 entries, little-endian                 |

`display_data` uses the layout `0xAARRGGBB` with alpha always `0xFF`.

### display_valid and display_anim semantics

**Static authored pixel** (`display_valid == 1`, `display_anim == 0`):
the modern renderer uses `display_data[idx]` as the ARGB colour directly.

**Animated pixel** (`display_valid == 1`, `display_anim == N` where N ≥ 1):
the pixel cycles through ramp N−1. `display_data[idx]` stores a **phase
offset** (0–ramp.color_count−1) that staggers pixels within the same ramp so
adjacent water pixels don't all flash in unison. The resolved colour is:

```
phase = (display_data[idx] + (cycles >> ramp.shift)) % ramp.color_count
colour = ramp.colors[phase]
```

**Palette-derived pixel** (`display_valid == 0`): the renderer falls back to
`pal32[material_id[idx]]` in both modes.

You can leave any subset of pixels unauthored. Unauthored pixels behave
exactly as in classic levels — they cycle with the active palette animation.

## Clear-on-Hit (v1)

When a projectile or explosion destroys terrain, the engine writes a new
`material_id` to the affected pixel (typically a background or air index)
**and clears `display_valid` to 0**. The authored ARGB is not restored.

This means:
- Blast craters are always palette-derived after the hit.
- The authored art for a pixel is a one-shot display: once the terrain is
  destroyed it falls back to the palette permanently (until the level is
  reloaded).

Stage 4 will add v2 mutations (scorch sprites writing new ARGB values). For
now, design authored terrain assuming hits revert to the palette.

## Palette-Cycling and Animated Terrain

OpenLiero animates several palette index ranges every tick. These ranges vary
by TC, but the built-in `openliero` TC animates:

- **168–174**: water shimmer (hardcoded; always active)
- Additional ranges driven by `common.color_anim` entries in `common.dat`

**If you author a pixel with `display_valid == 1` and `display_anim == 0`
(static authored), the palette animation stops for that pixel.** This is
usually undesirable for water.

You have two options for animated terrain in modern mode:

1. **Leave it unauthored** (`display_valid = 0`): uses the palette cycle as
   before (classic behaviour, no authored colour).
2. **Use a Stage 4 ramp** (`display_valid = 1`, `display_anim = N`): defines
   your own colour sequence independent of the palette.

### Worked example: static rock over palette-cycled water

```
Terrain               material_id   display_valid   display_anim   display_data
────────────────────────────────────────────────────────────────────────────────
Authored rock         idx = 3       1               0              0xFF4A3B2C
Palette-cycled water  idx = 170     0               (unused)       (unused)
```

The rock shows `0xFF4A3B2C` in modern mode; water continues to palette-cycle.

### Worked example: animated water ramp (Stage 4)

Define ramp 0 (index 1 in `display_anim`) as a 4-colour water sequence with
`shift = 0` (cycles every tick):

```
ramp_count = 1
ramp[0].shift = 0
ramp[0].colors = [0xFF1A3A6A, 0xFF2A4A7A, 0xFF1A3A6A, 0xFF0A2A5A]
```

Write `display_anim[idx] = 1` and `display_valid[idx] = 1` for each water
pixel. Set `display_data[idx]` to a phase offset (0–3) to stagger adjacent
pixels so they don't all change colour simultaneously:

```
Terrain         display_valid  display_anim  display_data (phase offset)
─────────────────────────────────────────────────────────────────────────
Water pixel A   1              1             0
Water pixel B   1              1             1
Water pixel C   1              1             2
```

At `cycles = 10`, pixel A resolves to `ramp[0].colors[(0 + 10) % 4]` = index
2 = `0xFF1A3A6A`. Pixel B resolves to index 3 = `0xFF0A2A5A`. Each pixel
is one step behind its neighbour, producing a gentle wave shimmer.

## Compatibility

- Classic and modern modes both load levels with or without the `MODERNLV`
  block. Absence means `display_valid` is all-zero: the level is visually
  identical to a classic level.
- Stage-3-format files (no `ramp_count` byte) load fine — the loader treats
  end-of-stream after `display_valid` as `ramp_count = 0`, producing an
  empty anim layer.
- The MODERNLV block (including the Stage 4 anim extension) is forwarded over
  netplay (protocol v8) and embedded in replays (replay format v9). Peers or
  replay files from before Stage 4 will not carry the anim layer; they load
  with an empty anim layer (static display only).
- In classic mode all display/anim data is ignored; the level renders from
  the palette as usual.
