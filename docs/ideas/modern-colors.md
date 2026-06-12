# Modern Colors

Design notes for evolving Open Liero's color pipeline beyond its 16-bit-era
constraints, in three independently-shippable stages, with a per-renderer
"color mode" toggle that can hot-swap between classic VGA and modern looks
in-game.

This is a design document. **Stage 1 is implemented** (see
`docs/plans/modern-colors-stage1.md` for the task-level record); its
section below describes what was actually built and what was learned.
Stages 2 and 3 remain unimplemented designs — each stands alone and can
be shipped or abandoned independently.

---

## How might we

How might we give Open Liero modern color expressiveness — starting with
player colors, eventually all graphics — without compromising the CRT-era
feel that defines its identity, and without breaking determinism, netplay,
or the TC asset pipeline?

---

## Background: what the current code actually is

A common misframing is that Open Liero is "16-bit color." It isn't:

- The framebuffer that reaches SDL is already `ARGB8888`
  (`gfx.cpp:370`, `gfx.cpp:378`).
- The `Color` struct in `gfx/color.hpp` is already 8 bits per channel.
- The whole game pipeline up to the final composition is **palette-indexed**:
  every sprite, every bitmap, the level itself, and the screen back buffer.
- The actual 16-bit-era constraint is the **6-bit VGA clamp** applied at
  palette load (`palette.cpp:63-65`: `entries[i].r = rgb[0] & 63`) and in
  `Palette::ScaleAdd` (`palette.hpp:26-33`).

So "support 32-bit colors" is really two separable things:

1. Lift the 6-bit clamp on per-channel palette values.
2. Allow some surfaces to bypass palette indexing entirely.

The level pixel value plays **two roles in one byte**: it's both a display
color (resolved via palette) and a material identity (used by collision,
destructibility, shadow-bake logic). `Level` keeps a parallel
`materials` array (`level.cpp:210`, `level.hpp:41`) but the source of truth
for what kind of terrain a pixel is, is its palette index. This conflation
is the load-bearing constraint that prevents true-color terrain without
explicit decoupling.

Three distinct palette-indexed surfaces exist, each for a different reason:

| Surface | Why palette-indexed | Stage that can change it |
|---|---|---|
| **Level** (`level.data`) | Pixel value *is* material ID — sim correctness | Stage 3 only |
| **Sprite assets** (`worm_sprites`, etc.) | Loaded from EXE/TC as 8-bit; `BlitImageOnMap` embeds sprite pixels *into* the level | Never |
| **Screen back buffer** (`Bitmap.pixels`) | Historical; just gets pal-resolved to ARGB in the final step (`gfx.cpp:911`) | Stage 2 |

Per-frame palette mechanics (`gfx.cpp:861-874`):

```cpp
play_renderer.pal = play_renderer.origpal;          // reset working palette
play_renderer.pal.RotateFrom(origpal, 168, 174, ...);  // animation effect
play_renderer.pal.SetWormColours(*settings);        // worm colors
play_renderer.pal.Fade(play_renderer.fade_value);   // fade
```

This pattern is critical: `origpal` is immutable; `pal` is rebuilt every
frame; then `pal.Activate(real_pal)` produces the live RGB array that the
`pal32[256]` LUT is built from. **All visual effects — fade, intro, death
flash, palette rotation, worm color updates — happen upstream of `pal32`
construction every frame.** This is what makes hot-toggle essentially free
in later stages: changing which palette `pal` is reset from is the only
thing needed; everything downstream just works.

Worm color implementation (`palette.cpp:69-94`): each worm owns a 5-entry
shaded ramp at hardcoded palette indices (worm 0: 30-34 + secondary copy at
0x58; worm 1: 39-43 + 0x78). Worm sprite art literally contains those
palette index values; "changing worm color" means rewriting those 5
palette entries. `SetWormColoursSpan` applies a hand-tuned 64-step gradient
(`38, 50, 64, 47, 64`) to derive the 5 ramp entries from a single base RGB.

---

## Recommended direction (three stages + two cross-cutting features)

| Stage | What ships | Estimated cost | Risk | Ship independently? |
|---|---|---|---|---|
| **Stage 1** — Modern Player Colors (**shipped**) | Unlock 6→8 bit per channel; per-worm `ColorBlock` indirection; `ColorMode` enum on Renderer; modern palette derived from the classic one (faithful, full-range); netplay protocol bump for 24-bit worm color | 1–2 days (actual: ~1 day + iteration) | None | Yes |
| **Stage 2** — ARGB Screen | Widen `Bitmap` to ARGB; convert ~30 blit primitive sites to `pal32[]` LUT stores; rewrite `ScaleDraw`; introduce `ShadowQuery` helper for the 8 shadow-inspector sites; introduce `Level::AppearanceAt()` accessor | ~12 days | Low | Yes |
| **Stage 3** — Full-Fidelity Terrain | Add `display_data` (ARGB) parallel layer to `Level`; make `AppearanceAt` mode-aware; modern level loader; snapshot `display_data` in `GameSnapshot` | ~9 days | Low | Yes (Stage 2 required) |
| **Hot-toggle** (cross-cutting) | F11 (or settings menu) swaps mode live; per-frame palette rebuild picks the right `origpal` | +0.5 / +0 / +0.5 days per stage | None | With Stage 1 |
| **Per-window mode** (cross-cutting) | Each `Renderer` has its own `mode`; player screen and spectator window can be in different modes simultaneously | +0.5 / +0 / +0 days per stage | None | With Stage 1 |
| **Toggle UX** | Settings menu entry + sticky preference + hotkey binding + (optional) cross-fade transition | +1 day, one-time | None | With Stage 1 |

**Cumulative roadmap with all features and the UX work: ~26 working days
(~5 weeks).** Each stage delivers independent user value; nothing requires
committing to the full vision up front.

---

## Stage 1 — Modern Player Colors (implemented)

### What shipped

Players pick worm colours with full 8-bit channels; classic rendering is
bit-identical to Liero 1.36; a per-renderer Classic/Modern colour mode can
be toggled live. As built:

- `Palette::entries` hold true 8-bit channels. Classic sources (the sprite
  TGA palette, level `POWERLEVEL` palettes, the tc_tool EXE reader) expand
  from the 6-bit VGA grid at load; `Activate` is a plain copy. Golden-value
  tests (`src/tests/test_palette.cpp`) pin classic output byte-for-byte.
- `WormSettings.rgb` is 0..255. Classic-mode worm shading quantizes back to
  the VGA grid inside `ScaleAdd` (bit-identical); Modern mode runs the same
  hand-tuned gradient at full 8-bit precision. Channel inputs are clamped —
  configs and net peers can carry out-of-range values.
- Old profiles/configs migrate via an explicit `rgbDepth = 8` TOML marker
  (files without it carry 0..63 channels and are expanded on load).
- Per-worm palette locations live in `ColorBlock { base, colour_index,
  status_index, width }` instead of scattered magic constants.
- `Renderer` carries `mode` plus two palette origins: `origpal` (classic;
  EXE/TC palette or a level's custom palette) and `origpal_modern`. Every
  per-frame palette rebuild resets from `Origpal()`, which picks by mode —
  including the netplay connect screens and the rollback pause/waiting
  overlay, which used to hardcode `exepal`.
- **The modern palette stays true to the original**: it is derived at load
  by `Palette::ExpandToFullRange()` — the classic palette mapped onto the
  full 8-bit range (`(v << 2) | (v >> 4)`, so the brightest VGA white is a
  true 255). A TC may override it by shipping a `modern.pal` (768-byte raw
  RGB, read unclamped); the stock game ships none.
- A level's custom palette wins in both modes. The palette itself already
  travels in the netplay level blob and the replay stream; the
  custom-palette flag is re-derived on arrival by comparing against the
  stock palette (`Level::DeriveHasCustomPalette`), so POWERLEVEL palettes
  render correctly in both modes on both peers and in replays.
- Toggle UX: `MODERN COLORS (F10)` in the options menu and an F10 hotkey
  flip the live mode instantly mid-game or in menus (the open menu's value
  strings are refreshed on toggle). The choice persists as `modernColors`
  in the TOML config; fresh installs default to Classic. F11 was the doc's
  suggestion but is taken by fullscreen; F9 by network player setup.
- The colour picker shows the original 0..63 numbering in classic mode
  (storage stays 0..255; `IntegerBehavior` gained a display divisor) and
  0..255 single-step in modern mode.
- Versions: netplay `kProtocolVersion` 5→6 (rgb was already `int32` on the
  wire — semantics-only bump), replay `kMyReplayVersion` 6→7, config
  `kConfigVersion` 3→4.

### What we learned (corrections to this document)

- **Replays are not unaffected.** The `.lrp` format embeds the cereal'd
  `Game` — including `level.origpal` — and re-serializes `Settings` /
  `WormSettings` mid-stream. The 8-bit widening therefore needed a replay
  version bump (6→7) with on-load expansion of pre-7 palettes and worm
  colours. Corollary: `modernColors` is serialized in the TOML config only;
  adding it to the binary `Settings` blob would have broken pre-7 replays.
- **"Visibly richer worm shading" was oversold.** Classic-vs-modern ramp
  math differs only in the 2 bits the VGA grid discards (≤ ~7/255 per
  channel), and a faithful full-range palette differs from classic by
  ≤ 3/255. With default colours the toggle is nearly invisible — *by
  design*. Modern mode's value is the unlocked picker and full-precision
  shading of off-grid colours, not a different look. (A saturation-boosted
  "vivid" palette was tried and explicitly rejected: the modern palette
  must stay true to the original.)
- **No curated palette ships.** "Modern Vivid" as a shipped data file was
  dropped in favour of deriving the modern palette in code (single source
  of truth); the `modern.pal` loader remains as the TC override hook.
  Multiple curated palettes (Muted, Colorblind-Safe, an actual Vivid…)
  remain a pure-data follow-up — see Option D below and load-bearing
  choice 2; the natural evolution is a palette *list* instead of the
  binary mode.
- The worm colour regions needed a third location beyond the doc's two:
  the minimap/status copies at 129/133 (now `ColorBlock::status_index`).
- Menu item value strings are rebuilt only on menu events; anything that
  changes display units (like the mode toggle) must refresh the open menu.

---

## Stage 2 — ARGB Screen

### Goal

Convert the screen back buffer (and `frozen_screen`) from palette-indexed
to ARGB. Keep the level and all sprite assets palette-indexed. This is the
prerequisite for true-color worms, true-color UI, alpha-blended particles,
and modern HUD elements. It does **not** touch terrain.

### What changes mechanically

The transformation is, at root, very simple: every blit that today does
`*dst = palette_index_byte` becomes `*dst = pal32[palette_index_byte]`,
and the destination pointer widens from `PalIdx*` to `uint32_t*`.

The `pal32[256]` LUT is rebuilt once per frame from the working palette
(already happens inside `ScaleDraw`; needs to be lifted to `Renderer`
scope so all blit primitives share it).

**Sites that change (uniformly mechanical):**

- `gfx/bitmap.hpp` — `Bitmap.pixels` widens from `unsigned char*` to
  `uint32_t*`. `GetPixel` returns `uint32_t&`. `Bitmap::Alloc` allocates
  `4 * pitch * height` bytes.
- `gfx/blit.cpp` — ~20 functions:
  - `Fill`, `FillRect`, `DrawBar` — `memset` becomes a `uint32_t` fill loop
    over `pal32[color]`.
  - `Vline`, `DrawLine`, `DASH`, single-pixel stores — `*p = color` becomes
    `*p = pal32[color]`.
  - `BlitImage*`, `DrawNinjarope`, `DrawLaserSight` — inner loops do
    `*dst = pal32[src_index]`.
  - `BlitImageNoKeyColour` — `memcpy(dst, src, width)` becomes a per-pixel
    loop with palette lookup. Slightly slower; fine.
  - `BlitImageOnMap`, `BlitStone` — **unchanged** (they write into the
    level, which stays palette-indexed).
- `gfx/font.cpp` — same pattern.
- `viewport.cpp` and `spectatorviewport.cpp` — single-pixel writes at
  `:323`, `:371`, `:482`, `:509` (viewport) and `:305`, `:353`, `:466`
  (spectatorviewport) become `*p = pal32[index]`.
- `gfx.cpp:902-911` — the final composition step. Today copies an
  intermediate palette-indexed surface to an ARGB SDL surface via
  palette lookup. In ARGB-screen world this becomes a direct
  `SDL_UpdateTexture` from the ARGB Bitmap.

### The non-mechanical part: `ScaleDraw`

`ScaleDraw` (`blit.cpp:626`) handles 1x / 2x / 3x / 4x upscaling for the
window. Today it takes `PalIdx* src` and a `pal32[]` parameter and does
the palette resolution during scaling. In Direction X:

```cpp
void ScaleDraw(uint32_t* src, int w, int h, std::size_t src_pitch,
               uint32_t* dest, std::size_t dest_pitch, int mag);
```

The `pal32` parameter goes away. The Scale2x filter macros in
`gfx/macros.hpp` (`SCALE2X`, `READER_8`, `WRITER_2X_8`) compare and store
8-bit pixels today; they need 32-bit variants (`READER_32`, `WRITER_2X_32`).
The `SHIFT_X` macro is width-agnostic.

One semantic subtlety: Scale2x compares neighboring pixel values to decide
whether to interpolate. In palette space, two palette entries with
identical RGB are *different* (different indices); in ARGB they're *equal*.
For a typical live palette this shouldn't change visible output, but
worth a side-by-side test on the legacy palette to confirm.

**Cost: ~1 day for the `ScaleDraw` rewrite + Scale2x macro variants. The
function ends up shorter than today (no per-pixel palette lookup).**

### The non-mechanical part: shadow inspector

The harder pattern is the 8 sites that read screen pixels as material:

| File | Line | Context |
|---|---|---|
| `blit.cpp` | 326 | `BlitShadowImage` (sprite shadow blit) |
| `blit.cpp` | 555 | `DrawShadowLine` (line shadow, e.g., ninjarope) |
| `viewport.cpp` | 331 | wobject single-pixel shadow |
| `viewport.cpp` | 379 | nobject single-pixel shadow |
| `viewport.cpp` | 490 | sobject single-pixel shadow |
| `spectatorviewport.cpp` | 313 | wobject (mirror of viewport.cpp) |
| `spectatorviewport.cpp` | 361 | nobject |
| `spectatorviewport.cpp` | 474 | sobject |

Each site does:

```cpp
if (common.materials[*screen_pixel].SeeShadow())
  *screen_pixel += 4;   // shift to palette entry that's the darkened version
```

This uses the screen as a queryable material surface and the palette as a
darken-by-jumping-4-entries table. Both break in ARGB world.

**The fix is uniform and clean.** The shadow is always cast at an offset
from the original draw — it falls on whatever was on the screen there
previously, which is almost always **terrain**. The level is the actual
source of truth for what material is at a world position. Query the level,
not the screen:

```cpp
struct ShadowQuery {
  Common const& common;
  Level const& level;
  uint32_t const* pal32;
  int world_offset_x, world_offset_y;  // screen + offset = world

  // Returns the ARGB to paint at screen pixel (sx, sy) if a shadow should
  // fall there, else 0 (caller skips).
  uint32_t ShadowedArgb(int sx, int sy) const {
    int wx = sx + world_offset_x;
    int wy = sy + world_offset_y;
    if (!level.IsInside(wx, wy)) return 0;
    PalIdx p = level.Pixel(wx, wy);
    if (!common.materials[p].SeeShadow()) return 0;
    return pal32[p + 4];   // ARGB of the shadowed dirt
  }
};
```

`BlitShadowImage` and `DrawShadowLine` grow `ShadowQuery const&` parameters.
The viewport/spectatorviewport call sites construct one `ShadowQuery` per
frame (they already have the world↔screen offset in scope from their own
coordinate math) and pass it down.

This abstraction is also exactly right for Stage 3: when terrain has an
ARGB display layer, the shadow result becomes a darkened version of
`display_data[wx, wy]` (e.g., multiplied by 0.5 in ARGB space) instead of
`pal32[material + 4]`. Same struct, different `ShadowedArgb` body.

**Cost: ~3 days for the helper, 8 site refactors, and visual diff testing.**

### What survives unchanged (verified)

- Determinism — renderer is fully downstream of `Game::processFrame`.
- Netplay — no rendering state crosses the wire.
- Replay format — replays don't store rendering; they re-render through
  the current pipeline.
- Fade, palette rotation, screen flash, intro fade — all happen on the
  working `Palette` before `pal32[]` is built; the per-frame rebuild
  pattern (`gfx.cpp:861-874`) is preserved exactly.
- All material lookups via `level.data` — verified at `level.hpp:22,41,46`,
  `level.cpp:210`, `rollbackController.cpp:128`, `worm.cpp:1097`,
  `common_model.hpp:497,594`. None of these inspect the screen.
- All sprite assets — `ReadSpriteTga` (`common.cpp:242,291`) is the only
  in-game sprite loader and remains palette-indexed. `IMG_Load`
  (`gfx.cpp:329,354`) only loads window icons, which never enter the
  blit pipeline.
- `frozen_screen` (`inputState.cpp:89`) — a `Bitmap` used for the input-
  state wipe effect. Widens with `Bitmap` automatically.
- `double_res` mode (`gfx.cpp:259-524`) — internal resolution is already
  configurable; ARGB surface allocation already exists; no change.

### Stage 2 cost breakdown

| Work item | Days |
|---|---|
| Widen `Bitmap` (single type — see decision below) | 1 |
| Convert ~20 blit primitives in `blit.cpp` | 4–5 |
| Convert `font.cpp` (same pattern) | 1 |
| Single-pixel viewport / minimap writes (7 sites) | 0.5 |
| `ShadowQuery` helper + 8 shadow site refactors | 3 |
| `ScaleDraw` rewrite + Scale2x macro variants | 1 |
| Plumb per-frame `pal32[]` from `Renderer` to blit primitives | 0.5 |
| Replace final composition (drop palette-lookup `SDL_UpdateTexture`) | 0.5 |
| Visual-correctness testing against legacy palette path | 2 |

**Total: ~12 working days.**

### Verification

- [ ] Render the same level/match in classic-palette mode pre- and
      post-Stage 2; pixel-diff the output (expected: identical or trivially
      different at Scale2x boundaries on the legacy palette).
- [ ] Fade-in and intro effects look identical.
- [ ] All eight shadow sites cast shadows in the same positions / colors
      as before.
- [ ] Minimap, weapon menu, stats screen, console text all render
      correctly.
- [ ] `double_res` mode works at 640×400 ARGB.
- [ ] Determinism test suite (`test_determinism`, `test_rollback_*`)
      still passes — sanity check that nothing leaked into the sim path.

---

## Stage 3 — Full-Fidelity Terrain

### Goal

Allow TC authors to ship levels with true-color terrain art. The level's
material identity (used by collision, destructibility) stays 8-bit; the
display data (what the screen shows) becomes ARGB.

### The core change: decouple material from color in `Level`

Today:

```cpp
struct Level {
  std::vector<PalIdx>   data;        // BOTH material ID AND color index
  std::vector<Material> materials;   // derived from data
  // ...
};
```

After Stage 3:

```cpp
struct Level {
  std::vector<uint8_t>   material_id;     // sim source of truth
  std::vector<Material>  materials;       // derived from material_id (unchanged)
  std::vector<uint32_t>  display_data;    // ARGB per pixel — true-color terrain
  std::vector<uint8_t>   display_valid;   // OR sentinel: 0 = no modern art, fall back
  // ...
};
```

The `display_valid` bit (or sentinel) is what makes the per-renderer mode
toggle work: classic-mode renderers always render palette-derived terrain;
modern-mode renderers render authored ARGB where present, palette-derived
elsewhere.

### Renderer integration via `Level::AppearanceAt`

Introduced in Stage 2 as the indirection layer between renderer and
level. In Stage 3 it becomes mode-aware:

```cpp
uint32_t AppearanceAt(int idx, ColorMode mode, uint32_t const* pal32) const {
  if (mode == Modern && display_valid[idx])
    return display_data[idx];           // authored modern art
  return pal32[material_id[idx]];       // palette-derived fallback
}
```

**Net effect:** classic-mode renderer sees Liero 1.36 exactly; modern-mode
renderer sees authored terrain where present, classic terrain elsewhere.
Hot-toggling between modes is instant — no `display_data` rebake needed.

### Level mutator updates

Level mutations (sprite-into-level writes, scorch, blood) need to maintain
both layers:

- `BlitImageOnMap` (`blit.cpp:292`) — writes `material_id` and `materials`
  as today; additionally writes `display_data` (if sprite source has modern
  art) or clears `display_valid` for that pixel.
- `bobject.cpp:34-40` (scorch / blood writes) — same pattern.
- `BlitStone` (`blit.cpp:337`) — same pattern.
- `level.SetPixel` — overload to optionally take an explicit ARGB color.

For the v1 of Stage 3, the simplest policy: **runtime mutations clear
`display_valid` for the affected pixels.** The modern-art appearance
applies only to terrain that hasn't been hit yet; once you shoot a hole or
splatter blood, that area falls back to palette-derived. Visually
inconsistent in the extreme but plausibly playable. Better authoring
(modern sprite art that writes ARGB into `display_data`) is a v2 of
Stage 3 if anyone cares.

### Rollback snapshot

`fast_snapshot.hpp:155-156` already shows the snapshot stores both
`level_data` and `level_materials` as parallel arrays. Adding
`display_data` and `display_valid` is incremental:

```cpp
struct GameSnapshot {
  // ... existing fields ...
  std::vector<uint8_t>  level_material_id;   // renamed from level_data
  std::vector<Material> level_materials;
  std::vector<uint32_t> level_display_data;  // new
  std::vector<uint8_t>  level_display_valid; // new
};
```

`SaveSnapshotFast` / `LoadSnapshotFast` (`game.cpp:615-676`) grow three
extra `memcpy` calls each.

For a 504×350 level (≈176K cells):

| Snapshot component | Per snapshot | × 8 frames |
|---|---|---|
| Existing (data + materials) | ~350–500 KB | ~3–4 MB |
| New `display_data` | 704 KB | 5.6 MB |
| New `display_valid` | 176 KB | 1.4 MB |
| Other (worms, projectiles, RNG, etc.) | ~10–50 KB | ~80–400 KB |

**~7 MB of additional resident memory.** Memcpy bandwidth at 50 Hz:
~45 MB/s. Modern memory bandwidth is ~30 GB/s. 0.15% — not measurable.

### Modern level authoring (the one real design decision)

Two reasonable formats:

**Option (a): Two-layer authoring (recommended).** The level file
declares an optional `display_layer` alongside the existing palette-indexed
material layer. If absent, `display_data` is derived via `pal32` at load
(visually identical to today). If present, the artist has painted a
richer surface. Existing TC packs continue to work unmodified.

**Option (b): Per-level palette extension.** Keep single-layer palette
indices but allow levels to ship a custom 1024-entry display palette
while keeping the 256-entry material palette. Cheaper for authors (one
image, just a richer palette); doesn't enable per-pixel true-color art,
just more colors than VGA.

This decision should be deferred until a TC author actually wants to ship
a modern level pack. If no one ever wants painted-art terrain, this whole
stage was wasted work — so its motivation needs to come from the TC
community, not engineering speculation.

### Stage 3 cost breakdown

| Work item | Days |
|---|---|
| Add `display_data` / `display_valid` to `Level`; rename `data` to `material_id` | 2 |
| Update `BlitImageOnMap`, scorch/blood mutators, `BlitStone` | 1 |
| Wire `display_data` into `GameSnapshot` (fast + cereal paths) | 1 |
| Update `Level::AppearanceAt` to be mode-aware + flip renderer to use it | 0.5 |
| Modern level loader (option (a) — two-layer format) | 2 |
| Update determinism / rollback tests for the new fields | 1 |
| Testing | 1.5 |

**Total: ~9 working days.**

### Verification

- [ ] Classic levels load and render identical to today in classic mode.
- [ ] Classic levels load and render identical to today in modern mode
      (no authored display layer → falls through to palette-derived).
- [ ] A test modern level with an authored ARGB display layer renders the
      authored art in modern mode and palette-derived art in classic mode.
- [ ] Shooting holes / splattering blood works in both modes (with the v1
      "runtime mutations clear `display_valid`" policy).
- [ ] Rollback tests pass with the larger snapshot.
- [ ] Replay format unaffected (replays render through the live pipeline).

---

## Cross-cutting feature: hot-toggle

A keypress — or a settings-menu entry — swaps `Renderer.mode` live. Next
frame uses the new mode.

**Stage 1 (implemented):** F10 hotkey + options-menu entry + sticky
`modernColors` setting. The per-frame palette rebuild picks the palette
origin via `Renderer::Origpal()`; the toggle is instant, including
mid-game.

**Stage 2:** automatically inherited; no additional cost. The `pal32[]`
LUT is rebuilt from the working palette every frame regardless of mode;
toggling `mode` just changes which `origpal` it derives from.

**Stage 3:** `Level::AppearanceAt(idx, mode, pal32)` already branches on
`mode`; hot-toggle just flips the branch. No `display_data` rebake
required. **+0.5 days** for the branching logic (subsumed if Stage 1's
hot-toggle is in).

UX considerations:

- **Where the toggle lives:** ship all three (a settings default, an
  in-game hotkey, a sticky "remember last choice"). ~1 day of one-time
  UI work shared across stages.
- **Transition feel:** the simplest implementation is an instant snap.
  Optional: piggyback on the existing `fade_value` to do a one-frame
  fade-out → palette swap → fade-in. ~0.5 days extra if desired.
- **During heated gameplay:** mid-match hot-toggle is fine — it's purely
  cosmetic and the player initiated it.

---

## Cross-cutting feature: per-window mode

Each `Renderer` already owns its own `origpal`, `pal`, `bmp`, SDL surface,
SDL texture, and `fade_value`. The play renderer, the single-screen
renderer, and the spectator renderer are already independent instances
(`gfx.cpp:861-874` shows the duplication). They just happen to be fed the
same source data today.

**Stage 1 (field implemented):** `mode` is a per-renderer field and every
rebuild reads it. The current UI sets the play and single-screen renderers
together; per-window controls (e.g. a separate spectator toggle) are not
built yet but need no further plumbing.

**Stage 2:** automatic — each renderer has its own `pal32[]` LUT already.

**Stage 3:** automatic — `AppearanceAt(idx, mode, pal32)` is called with
the rendering renderer's mode and `pal32`. Player screen renders classic;
spectator renders modern. They share the same `Level` (sim is identical)
but render its `display_data` (or palette-derived terrain) according to
their own settings.

Configurations this unlocks:

- Player screen modern, spectator screen classic (streamer wants a
  nostalgic spectator overlay).
- Split-screen with each half in a different mode.
- "Color mode" cycled with a hotkey during gameplay for direct A/B
  comparison.

Worm-color truncation across modes: if a player picks a vivid 8-bit-per-
channel color and a classic-mode renderer displays their worm, that
renderer's `SetWormColour` truncates to 6 bits per channel (`>> 2`). The
modern renderer uses the full 8 bits. The truncation is what the current
code does today; no new code path required.

Color mode and netplay are fully orthogonal: mode is local display state,
never crosses the wire, never appears in snapshots or replays. Two players
in a netplay session can each be in whatever mode they like.

---

## Load-bearing forward-looking design choices

These are the small choices in early stages that materially affect the
ease of later stages. Worth making intentionally rather than discovering
them later.

### 1. `ColorMode` lives on `Renderer`, not on `Settings`

Settings hold the *default*; the renderer holds the *live* mode. This is
what makes both hot-toggle and per-window mode trivial extensions of
Stage 1 rather than refactors of the palette ownership later. ~5 extra
lines in Stage 1.

### 2. Per-renderer `origpal_classic` and `origpal_modern`

Both palettes load at startup, live alongside each other on the
`Renderer`. The mode flag picks which one `pal` is reset from each frame.
Trivial in Stage 1; the natural extension point for shipping additional
curated palettes ("Muted," "Colorblind-Safe") later as pure data, no code
changes needed.

### 3. `pal32[256]` LUT lives on `Renderer` at frame scope

Today it's reconstructed inside `ScaleDraw`. Lift it to `Renderer` and
compute it once per frame after the working palette is finalized. In
Stage 2 every blit primitive uses it; in Stage 3 `AppearanceAt` uses it
for the palette-derived fallback. Centralizing once prevents drift later.

### 4. Widen `Bitmap` directly (don't introduce a separate `ScreenBitmap` type)

`frozen_screen` (`inputState.cpp:89`) is a `Bitmap` that snapshots the
screen for menu effects. If we have two Bitmap types, every helper that
operates on "any kind of Bitmap" needs templating. Widening `Bitmap`
directly means `frozen_screen` widens for free. The level's data array
is already `std::vector<PalIdx>` (not a `Bitmap`), so the level/screen
separation doesn't depend on having two Bitmap types.

### 5. `Level::AppearanceAt(idx, ...)` accessor introduced in Stage 2

Even though in Stage 2 it just returns `pal32[data[idx]]`. Five extra
lines. Makes Stage 3 a one-line renderer change instead of a
search-and-replace across the level-to-screen blits.

### 6. `ShadowQuery` helper introduced in Stage 2

Designed up front to be extended in Stage 3. Stage 2's `ShadowedArgb`
returns `pal32[material + 4]`; Stage 3's returns a darkened
`display_data[wx, wy]` or falls through to the palette path. Same struct,
same call sites.

### 7. `ColorBlock { base, width, ramp }` for worm color regions in Stage 1

Replaces the magic constants `kWormSpriteColorBase[2] = {32, 41}` and the
hardcoded `30-34` / `39-43` literals. Even if all we ship in Stage 1 is
the defaults that preserve current behavior, the indirection unlocks
custom skins, wider ramps, and per-skin palette allocation as data-level
changes later.

---

## Alternatives considered and rejected

This section preserves the decision rationale for options that came up
during design and were not picked. Listed roughly in the order they were
discussed.

### Option B — Richer palette ramp without ARGB screen

Expand the worm color ramp from 5 entries to e.g. 16, re-shade the worm
sprite to use the wider ramp, allocate more palette real estate per worm.
**Rejected.** Worst ratio in the evaluation matrix:

- User value: low. Players are unlikely to perceive ramp smoothness; the
  visible difference between 5-step and 16-step shading on a 16×16 sprite
  is marginal.
- Cost: medium. Sprite re-shading is fiddly; palette real estate is
  tight (256 entries total, with fire/explosion ramps already occupying
  much of it).
- Differentiation: low.

**However:** the `ColorBlock` indirection in Stage 1 leaves this option
*architecturally available* — a future skin pack could declare a wider
ramp width. We just don't ship it as a baseline.

### Option C-true — True-color worm sprites without ARGB screen

Render the worm sprite into ARGB on the fly and blit with alpha on top of
the palette-indexed screen. **Rejected.** The screen is fundamentally
palette-indexed up to the final composition step. To composite a true-
color worm onto a palette-indexed screen, you'd need either:

- **Post-pass compositing** after the palette→ARGB conversion. Breaks
  worm participation in terrain z-order (worms drawn after dirt thrown in
  front of them), and breaks palette-based effects (fade, intro, death
  flash) because those modify palette entries, not ARGB pixels. Visual
  semantics change in subtle ways.
- **Hybrid blit path** with palette destination + true-color source +
  alpha. Introduces a code branch and a second pixel format inside the
  same surface. Painful to maintain.

**Replaced by:** Stage 2 (ARGB screen). Once the screen is ARGB, true-
color worms become a clean follow-up: pre-bake or live-bake an ARGB sprite
set from the chosen color, blit with alpha. No structural objection
remains. So if true-color worms are wanted, Stage 2 is the right path,
not C-true done in isolation.

### Option D-only — Curated palettes with no code changes

Ship 2-3 curated 256-color palettes (Classic VGA, Modern Vivid, Muted,
Colorblind-Safe) and let the player switch. No engine changes; just data.
**Rejected as a standalone v1.** Stage 1 as shipped does *not* include a
curated palette either — the modern palette is derived from the classic
one and stays true to the original (see Stage 1 learnings). Curated
palettes remain the natural follow-up: the `modern.pal` per-TC override
hook exists, and generalizing the binary Classic/Modern mode into a
palette list is a small, mostly data-driven change.

### Option E — Accessibility / streamer picker UI

Re-cast the feature as "player distinguishability": HSL picker, colorblind-
safe presets, automatic contrast check against the level background.
**Rejected as not the primary feature**, but **flagged as a reasonable
follow-up PR.** Genuinely valuable but separate from the color-fidelity
work — bundling them delays both. Could ship after Stage 1 as a UI-only
change against the unlocked picker.

### Snapshot strategy 2 (Stage 3) — Derive display data from material at snapshot/restore time

Don't snapshot `display_data`; recompute it from `material_id` on restore.
**Rejected.** This works for classic palette-derived levels but falls
apart for authored modern art (you can't derive a hand-painted scorch
texture from a material ID). Would force modern levels to give up either
true-color terrain or rollback support.

### Snapshot strategy 3 (Stage 3) — Compact "decoration overlay" instead of full ARGB layer

Snapshot only `material_id` + an 8-bit "decoration" overlay (per-pixel
index into a per-level decoration palette). **Rejected as premature
optimization.** The full ARGB layer costs ~7 MB extra resident memory in
the rollback ring. Modern hardware shrugs. No measured pressure to
optimize.

### Separate `ScreenBitmap` type vs. widening `Bitmap`

Introduce a `ScreenBitmap` (ARGB) alongside the existing palette-indexed
`Bitmap`. Type system enforces "you can't accidentally treat a palette
buffer as ARGB or vice versa." **Rejected.** `frozen_screen` is a `Bitmap`
that snapshots the screen; with two types, helpers that operate on either
would need templates. The level's `data` is already `std::vector<PalIdx>`
(not a `Bitmap`), so the conceptual separation between "level pixels"
and "screen pixels" doesn't depend on having two `Bitmap` types. Widening
the single type is simpler and ergonomic.

### True-color level data without the toggle

Skip Stage 1's classic-mode preservation and just convert everything to
true-color. **Rejected.** Loses the "old-school authenticity" feature the
user wanted to preserve. Also breaks bit-identical replay rendering of
classic TC packs.

### More per-feature toggles (per-sprite, per-particle, per-font)

Have separate switches for player colors, level rendering, particles,
fonts. **Rejected as premature.** Each toggle multiplies the test matrix
exponentially. Start with one toggle (Classic vs Modern color mode), add
finer granularity only if a real use case demands it. The current toggle
already covers the main user goals (purist players + modernizers + per-
window configurations).

### Convert the level itself to ARGB and decouple material from collision

This was the original framing of "all graphics modernized." **Rejected as
premature.** Stage 3's "decouple material from display" already gets the
visual fidelity goal without touching the sim. The level's `material_id`
stays as the sim source of truth; only the display layer changes. There
is no scenario where the sim itself benefits from ARGB level pixels.

### Post-pass HUD/UI compositing in true color while keeping the rest palette-indexed

Render HUD/menus into a separate ARGB layer and overlay on the final
ARGB texture. Skip Stage 2 entirely. **Rejected.** Possible in principle
but the HUD draws through the same blit primitives as gameplay, so the
two paths would have to coexist. Cleaner to make the whole screen ARGB
once (Stage 2) than to maintain a hybrid.

---

## Open questions

These need answers before specific stages can be committed to. They're
not blockers for considering the roadmap; they're decisions deferred to
when each stage's work actually starts.

### Stage 1 (resolved during implementation)

- Wire protocol: `PlayerInfo.rgb` was already `int32[3]`; only semantics
  changed. `kProtocolVersion` (a clean version byte in the handshake)
  bumped 5→6; mismatched peers refuse each other.
- TOML worm colour: widened to 0..255 with an explicit `rgbDepth = 8`
  marker; old files (no marker) are expanded ×4 on load and out-of-range
  values are clamped.

### Stage 2

- **`Bitmap` size doubles.** Memory pressure check for the lowest-end
  Emscripten target: the play screen is 320×200 (or 640×400 at
  `double_res`), so 128 KB → 512 KB at base or 512 KB → 2 MB at high res.
  Plus `frozen_screen` and the spectator surface. Total: a few MB extra.
  Trivial on desktop; worth a sanity check on the Emscripten build.
- **`Scale2x` filter semantic on the legacy palette.** Pixel equality in
  ARGB vs palette index could theoretically affect Scale2x output. Visual
  diff during testing.

### Stage 3

- **Modern level authoring model: option (a) two-layer vs (b) per-level
  extended palette.** Decide once a TC author actually expresses interest.
  Until then this whole stage is speculative.
- **Runtime mutation policy.** v1 proposal: scorch/blood writes clear
  `display_valid` for affected pixels, falling back to palette-derived
  appearance for those areas. Acceptable visual inconsistency? Or do we
  want sprite-source-derived ARGB writes from the start?
- **Compatibility of existing TC level files.** Existing TC archives are
  palette-indexed throughout; they should load with no modern display
  layer present and render identically to today. Confirm during testing.

### Cross-cutting

- **Two-viewport-renderer duplication** (`viewport.cpp` and
  `spectatorviewport.cpp` mirror each other for shadow rendering and
  single-pixel particle draws). Not a blocker; worth fixing before
  piling on more per-renderer features. Flag as a related cleanup, not
  part of this work.
- **Color mode UX.** Settings menu entry + hotkey + sticky preference is
  the recommended UX; default mode for new installs (Classic? Modern?)
  is a product call.

---

## Not doing (explicitly out of scope)

These were considered and explicitly excluded — not "we forgot," but
"we decided no."

- **True-color sprite assets.** Sprite TGA loading (`ReadSpriteTga`)
  stays palette-indexed. TC asset format unchanged. Skins, if they
  happen, use the `ColorBlock` indirection to allocate palette real
  estate, not new asset formats.
- **True-color level material identity.** `material_id` stays 8-bit;
  collision and destructibility key off it; ~10-20 distinct materials
  total. No need for more.
- **Replay format changes.** Replays don't snapshot rendering state;
  they re-render through the current pipeline. A replay recorded pre-
  Stage 2 plays back identical post-Stage 2 in classic mode; post-Stage
  3 it can also be played back in modern mode against a modern level
  pack. No format change.
- **Per-feature toggles for "modernize just the level" or "modernize
  just the particles."** Premature. The Classic/Modern mode flag covers
  the visible user goals; finer granularity adds test-matrix explosion
  without proven demand.
- **GPU-side rendering (move blits to shaders).** Different project. The
  CPU-side palette-and-blit architecture is fine for Liero's resolution
  and frame rate; no performance problem motivating a GPU rewrite.
- **Migration of the `data/` palette format.** The classic palette file
  format stays exactly as it is. The modern palette is a new file in
  the same format, just loaded without the 6-bit clamp.
- **Cross-fade animation between palettes.** Initially out; can be added
  later as a UX polish (~0.5 day) using the existing `fade_value`
  machinery if desired.
- **Loading community-contributed palettes from disk.** Initially out;
  a TC can override the derived modern palette with a `modern.pal`. If
  demand exists, a user-facing palette list is a data-level change later.

---

## Summary

This roadmap is structured so that every stage delivers user-visible
value on its own, every stage is independently shippable, and every
later stage is meaningfully cheaper because of the architectural
prep made by earlier stages. The two speculative cross-cutting features
(hot-toggle, per-window mode) cost almost nothing because the existing
renderer architecture is already per-instance — a happy accident worth
preserving.

**Stage 1 is implemented**, including the hot-toggle and the per-renderer
mode field. It pays the forward-looking architectural cost (`ColorMode`
on `Renderer`, `ColorBlock` indirection, per-renderer palette origins)
that makes everything else cheap if/when it's wanted.

Stages 2 and 3, per-window toggle UI, and curated palette options can be
revisited when player or TC-author demand makes their cost worth paying.
The homework in this document is intended to make that revisit cheap.
