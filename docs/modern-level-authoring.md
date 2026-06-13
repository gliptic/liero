# Modern Level Authoring (Stage 3)

This guide describes how to author true-color terrain art for OpenLiero TC
packs using the Stage 3 display layer.

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
["MODERNLV" + display_data + display_valid]  ← optional (Stage 3 display layer)
```

The `MODERNLV` block, if present, must appear at the end of the file, after
the optional `POWERLEVEL` block.

### MODERNLV block layout

| Field          | Size (bytes)      | Description                            |
|----------------|-------------------|----------------------------------------|
| magic          | 8                 | ASCII literal `MODERNLV`               |
| display_data   | 504 × 350 × 4     | ARGB32, little-endian, row-major       |
| display_valid  | 504 × 350         | 1 = authored (use display_data), 0 = palette fallback |

`display_data` uses the layout `0xAARRGGBB` with alpha always `0xFF`.

### display_valid semantics

- `display_valid[x + y*504] == 1`: the pixel is **authored**. The modern
  renderer uses `display_data[x + y*504]` as-is.
- `display_valid[x + y*504] == 0`: the pixel is **palette-derived**. The
  renderer falls back to `pal32[material_id[x + y*504]]` in both modes.

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

**If you author (`display_valid == 1`) a pixel whose material index falls in
an animated range, the animation stops for that pixel** — the authored ARGB
is static. This is usually undesirable for water.

**Rule: leave palette-animated terrain unauthored.** Set `display_valid = 0`
for any pixel you want to keep cycling. Only author pixels that represent
static art (rock, metal, etc.).

### Worked example: rock over water

```
Terrain               material_id   display_valid   display_data
─────────────────────────────────────────────────────────────────
Authored rock         idx = 3       1               0xFF4A3B2CU
Palette-cycled water  idx = 170     0               (unused)
```

In modern mode the rock pixel shows `0xFF4A3B2C`; the water pixel continues
to cycle through the animated palette. In classic mode both use the palette.

## Compatibility

- Classic and modern modes both load levels with or without the `MODERNLV`
  block. Absence means `display_valid` is all-zero: the level is visually
  identical to today.
- The MODERNLV block is forwarded over netplay (Stage 3 protocol v7) and
  embedded in replays (replay format v8). Peers or replay files from before
  Stage 3 will not carry the display layer; they load as classic levels.
- The authoring rules in this guide cover Stage 3 only. **Animated true-color
  terrain** (authored ARGB that cycles on its own schedule) is deferred to
  Stage 4, which will extend this same file format.
