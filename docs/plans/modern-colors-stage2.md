# Implementation Plan: Modern Colors — Stage 2 (ARGB Screen)

**Status: implemented** (branch `modern-colors-stage2`).

Deviations from the plan, found necessary during implementation:

- **Fade applies at composition, not in the palette rebuild.** `pal.Fade`
  on the working palette would bake the fade level into every ARGB capture
  (weapsel backgrounds, the menu's captured game frame, `frozen_screen`
  dialogs), freezing them at the brightness of the capture frame. The
  identical per-channel math (`(v * amount) >> 5`) now runs in `ScaleDraw`
  using `renderer.fade_value` — value-identical for live content and
  byte-identical to the old composition-time behaviour for frozen content.
- **`Game::Draw` repaints the background after the palette rebuild.**
  Callers clear before drawing, which resolves entry 0 through the
  previous frame's LUT — a one-frame lag on border pixels during screen
  flashes (caught by a per-frame hash diff).
- **Verification used `framehash`** (new headless per-frame output hasher,
  `src/tests/framehash_main.cpp`) instead of videotool diffs: the video
  tool was broken on master (never wired `ReplayReader::game`, unguarded
  `worms[1]`) and encodes lossily. Both bugs are fixed in this branch.
- **Accepted minor visual change** beyond the documented shadow semantics:
  palette *rotation* (water shimmer) and live worm-colour previews no
  longer animate inside frozen captures (weapsel backgrounds, the menu's
  captured game frame) because those pixels are baked ARGB. Fades still
  apply (see above).

Task breakdown of Stage 2 of `docs/ideas/modern-colors.md`: convert the
screen back buffer (`Renderer.bmp`, `frozen_screen`) from palette-indexed
to ARGB. The level and all sprite assets stay palette-indexed. Classic-mode
output must stay pixel-identical, except for the shadow-semantics change
accepted by the design doc (Phase 2 below). Renderer-only: no sim, netplay,
replay, or snapshot changes, so no version bumps.

## Corrections to the design doc (found during planning)

Grounding the plan in the current tree found five places where the code
differs from `docs/ideas/modern-colors.md`'s Stage 2 section:

1. **The Scale2x rewrite is moot.** `SCALE2X` / `FILTER_X` / `READER_8` /
   `WRITER_2X_8` in `gfx/macros.hpp` are dead code — zero call sites.
   `ScaleDraw` (`blit.cpp:671`) is plain nearest-neighbor magnification.
   The doc's ~1 day "Scale2x macro variants" item and the pixel-equality
   semantic concern both disappear. The dead macros get deleted.
2. **A 9th screen-as-material site the doc missed.** `BlitImageR`
   (`blit.cpp:278`) draws sobject explosion pixels only where the
   *destination* pixel is in palette range [160,168) — called from
   `viewport.cpp:288` and `spectatorviewport.cpp:269`. Needs the same
   query-the-level fix as the 8 shadow sites.
3. **Palette finalization currently happens *after* drawing.** `Game::Draw`
   (`game.cpp:177-187`) rebuilds `pal` after the viewports are drawn;
   `MenuFlip` (`gfx.cpp:950`) rebuilds after `state_stack.Draw()`. Today
   that's fine because the palette is only consumed at composition. In ARGB
   world every blit consumes `pal32`, so finalize-before-draw reordering is
   required. There is exactly **one** frame driver (`Gfx::RunOneFrame`,
   `gfx.cpp:1455-1464`) plus three local rebuild blocks
   (`weapsel.cpp:132,184`, `rollbackController.cpp:1233-1237`), so the
   refactor is contained — and behavior-neutral *before* the widening, so
   it lands and is verified first.
4. **`BlitImageNoKeyColour` has two distinct semantics** that must split:
   level→screen draw (`viewport.cpp:196`, `spectatorviewport.cpp:178` —
   palette source, becomes the `Level::AppearanceAt` consumer) vs.
   ARGB→ARGB restore from `frozen_screen` (`inputState.cpp:94`).
5. **Extra palette-index writers the doc didn't list:**
   `Level::DrawMiniature` (`level.cpp:339`, writes indices into a Bitmap;
   4 call sites), `DrawHeatmap` / `BLIT3` (`blit.cpp:665`), and
   `predictive_ai.cpp:563`. The **video tool**
   (`replay_to_video.cpp:123-134`) consumes `ScaleDraw` +
   `PreparePaletteBgra` and must be converted alongside.

Both SDL surfaces are fixed `SDL_PIXELFORMAT_ARGB8888` (`gfx.cpp:383,402`),
so one canonical pal32 packing (`0xFF000000 | r<<16 | g<<8 | b`) replaces
both `Gfx::PreparePalette` (SDL_MapRGB) and `PreparePaletteBgra`.

## Architecture decisions

1. **`pal32[256]` lives on `Renderer` at frame scope** (design doc
   load-bearing choice #3), rebuilt by `Renderer::UpdatePal32()` at the end
   of every palette-rebuild block. `pal.Activate` moves out of `Gfx::Draw`
   into `UpdatePal32`.
2. **`Bitmap` gains a `uint32_t const* pal32` member** pointing at its
   renderer's LUT (rebuilt in place each frame — the pointer never
   dangles; `frozen_screen`'s is set at init). Blit primitives resolve
   indices via `scr.pal32[c]` internally, so `Bitmap::SetPixel(x, y,
   PalIdx)` and every menu/state call site keep their signatures. This is
   the doc's "plumb pal32 from Renderer to blit primitives" with roughly
   half the churn of explicit parameters.
3. **Frame ordering becomes finalize-palette → draw → compose.** Today's
   rebuild-after-draw works only because the palette is applied at
   composition. The reorder is value-identical: rotation uses `cycles` /
   `menu_cycles`, fades use `fade_value`, and none of those mutate during
   drawing.
4. **Screen-as-material reads become level queries** via a `ShadowQuery`
   helper struct (design doc Stage 2 section): `{ common, level, pal32,
   world_offset }` with `ShadowedArgb(sx, sy)` (returns `pal32[p + 4]` or
   0) and `PixelAt(sx, sy)` (for `BlitImageR`'s range check). The
   viewports already have the world↔screen offset (`kOffs`) in scope.
   Accepted semantic change: shadows/explosion-masks key off terrain, not
   whatever sprite was drawn earlier on the screen.
5. **`Bitmap` widens in place** (no separate `ScreenBitmap` type — design
   doc load-bearing choice #4). `pitch` stays in pixels; all
   `memset`/`memcpy` byte-count sites are converted explicitly.
6. **`Level::AppearanceAt(idx, pal32)` is introduced now** (design doc
   load-bearing choice #5), even though in Stage 2 it just returns
   `pal32[data[idx]]`. It is the Stage 3 seam.
7. **Canonical pal32 sets A=0xFF.** SDL textures ignore alpha here; the
   video tool's BGRA path treats 0xFF as opaque (it previously got A=0
   from `PreparePaletteBgra` — verify output during Phase 4).

## Task list

### Phase 1: Palette-finalize-before-draw reorder (behavior-neutral)

#### Task 1: `Renderer::pal32` + `UpdatePal32()`, reorder rebuild blocks

**Description:** Add `uint32_t pal32[256]` and `UpdatePal32()` to
`Renderer` (packs from `pal.Activate`). Move the rebuild block in
`Game::Draw` above the viewport draws. Split `MenuFlip` so the
fade/rotation/worm-colour block runs before `state_stack.Draw()` in
`RunOneFrame` (composition stays after). Reorder the `weapsel.cpp` rebuild
blocks before their draws; add `UpdatePal32()` after the rollback pause
overlay rebuild (its draws already follow it). Call `UpdatePal32()` at the
end of every rebuild block.

**Acceptance criteria:**
- [ ] Every `pal` rebuild block ends with `UpdatePal32()` and precedes all
      drawing into that renderer's bmp for the frame (grep audit:
      `RotateFrom|SetWormColours|\.Fade\(|LightUp`).
- [ ] Output is pixel-identical to before (same inputs, same values —
      verified by videotool frame diff against master on a replay that
      includes fades and palette rotation).

**Verification:** build + `ctest`; determinism suites; videotool diff;
smoke-launch.

**Dependencies:** None. **Files:** `src/game/gfx/renderer.{hpp,cpp}`,
`src/game/game.cpp`, `src/game/gfx.cpp`, `src/game/weapsel.cpp`,
`src/game/controller/rollbackController.cpp`. **Scope:** M.

### Phase 2: Screen-as-material reads become level queries (still 8-bit)

#### Task 2: `ShadowQuery` helper + 9 site refactors

**Description:** Introduce `ShadowQuery` (new header next to `blit.hpp`).
Convert the 8 shadow sites — `BlitShadowImage` (`blit.cpp:344`),
`DrawShadowLine` (`blit.cpp:589`), and the single-pixel sites at
`viewport.cpp:353,405,522` / `spectatorviewport.cpp:334,386,505` — to
query the level instead of the screen. Convert `BlitImageR`'s
dest-range check to `PixelAt`. While the screen is still 8-bit, the
sites write the shadowed *index* (`p + 4`); the ARGB write lands in
Phase 3. `BlitShadowImage`, `DrawShadowLine`, and `BlitImageR` grow
`ShadowQuery const&` parameters; viewports construct one per frame from
`kOffs`.

**Acceptance criteria:**
- [ ] No reads of screen pixels as material remain (grep:
      `materials\[.*GetPixel|materials\[\*|rowdest - 160`).
- [ ] New `test_blit` covers `ShadowQuery` against a synthetic level
      (shadow falls on SeeShadow terrain, skipped elsewhere, clipped
      outside bounds) and the `BlitImageR` range check.
- [ ] Visual side-by-side: shadows in the same positions; deltas only
      where a shadow/explosion overlaps a previously drawn sprite
      (documented, accepted).

**Verification:** build + `ctest` incl. new `test_blit`; videotool diff
(expected: near-identical, documented shadow deltas only); smoke-launch.

**Dependencies:** Task 1 (pal32 exists for the struct; the reorder keeps
phases independently diffable). **Files:** `src/game/gfx/blit.{hpp,cpp}`,
new `src/game/gfx/shadow_query.hpp`, `src/game/viewport.cpp`,
`src/game/spectatorviewport.cpp`, `src/tests/test_blit.cpp`,
`CMakeLists.txt`. **Scope:** M.

### Checkpoint A (after Tasks 1–2)
- [ ] Full test suite green; videotool diff clean apart from accepted
      shadow deltas; game smoke-launches; screen still palette-indexed.

### Phase 3: The widening (one atomic commit series)

#### Task 3: Widen `Bitmap` to ARGB and convert every index writer

**Description:** `Bitmap.pixels` → `uint32_t*`; add the `pal32` member;
`GetPixel` returns `uint32_t&`; `SetPixel(x, y, PalIdx)` resolves via
`pal32`; `Alloc`/`Copy` byte counts ×4. Convert all primitives in
`blit.cpp` (`Fill`/`FillRect`/`DrawBar` memsets → uint32 fill loops;
`Vline`, `DASH`, `DrawLine`, `DrawNinjarope`, `DrawLaserSight`,
`BlitImage*`, `BlitFireCone`, `DrawHeatmap` and the `BLIT`/`BLIT2`/`BLIT3`
macros → `pal32[c]` stores) and `font.cpp:DrawChar`. Level writers
(`BlitImageOnMap`, `BlitStone`, `DrawDirtEffect`, `CorrectShadow`, `BLITL`)
untouched. Split `BlitImageNoKeyColour` into `DrawLevel` (consuming new
`Level::AppearanceAt(idx, pal32)`) and an ARGB `BlitBitmap` for the
`frozen_screen` restore in `inputState.cpp`. Convert the remaining index
writers: minimap blips (`viewport.cpp:544-566`), wobject/nobject/sobject
single-pixel draws, `Level::DrawMiniature` (via `dest.pal32`),
`predictive_ai.cpp:563`. Phase 2's shadow sites switch from writing
`p + 4` to `ShadowedArgb`.

**Silent-bug hazard:** after widening, a stray `GetPixel(x, y) = index`
still compiles. Definition of done includes a tree-wide audit of every
`GetPixel`/`SetPixel`/`.pixels` use and every `memset`/`memcpy` touching
bitmap memory.

**Acceptance criteria:**
- [ ] Audit checklist complete; no index value is ever stored to the
      screen un-resolved.
- [ ] `test_blit` extended with ARGB golden expectations for
      representative primitives and `AppearanceAt`.
- [ ] Build green for the whole tree including tests and tools.

**Verification:** build + `ctest`; the game will not compose correctly
until Task 4, so Tasks 3+4 merge together; smoke verification happens
after Task 4.

**Dependencies:** Tasks 1–2. **Files:** `src/game/gfx/bitmap.hpp`,
`src/game/gfx/blit.{hpp,cpp}`, `src/game/gfx/font.cpp`,
`src/game/gfx/renderer.cpp`, `src/game/level.{hpp,cpp}`,
`src/game/viewport.cpp`, `src/game/spectatorviewport.cpp`,
`src/game/inputState.cpp`, `src/game/ai/predictive_ai.cpp`,
`src/tests/test_blit.cpp`. **Scope:** L.

#### Task 4: Composition + `ScaleDraw` + video tool

**Description:** `ScaleDraw` becomes `uint32_t* → uint32_t*`
nearest-neighbor (drops the pal32 parameter and the packed-4-bytes trick;
mag=1 becomes row memcpy). `Gfx::Draw` drops `pal.Activate` +
`PreparePalette` and feeds the ARGB bmp straight through `ScaleDraw` to
the surface/texture. Video tool drops `PreparePaletteBgra` and consumes
the ARGB bmp directly. Delete `PreparePalette`, `PreparePaletteBgra`, and
the dead Scale2x macros in `macros.hpp`.

**Acceptance criteria:**
- [ ] Game renders correctly at mag 1..4 (window resize) and in
      `double_res`.
- [ ] Spectator window renders correctly.
- [ ] Video tool renders a replay with correct colours (channel order +
      alpha checked).
- [ ] Dead code gone (`SCALE2X`, `FILTER_X`, `READER_8`, `WRITER_2X_8`,
      `SCALE2X_DECL`, both Prepare* functions).

**Verification:** build + `ctest`; videotool render; smoke-launch.

**Dependencies:** Task 3 (same merge). **Files:**
`src/game/gfx/blit.{hpp,cpp}`, `src/game/gfx/macros.hpp`,
`src/game/gfx.{hpp,cpp}`, `src/video_tool/replay_to_video.cpp`.
**Scope:** M.

### Phase 4: Verification

#### Task 5: Full verification sweep

**Acceptance criteria (from the design doc's Stage 2 checklist):**
- [ ] Videotool frame diff vs. master on the same replay in classic mode:
      identical output apart from the Phase 2 shadow deltas.
- [ ] Fade-in, intro, death flash, palette rotation identical.
- [ ] All nine shadow/material sites render shadows in the same
      positions/colours.
- [ ] Minimap, weapon menu, stats screen (incl. heatmap), console text,
      Save As dialogs (frozen_screen wipe) render correctly.
- [ ] `double_res` works at 640×400 ARGB.
- [ ] F10 classic/modern hot-toggle still instant mid-game and in menus.
- [ ] `test_determinism`, `test_rollback_*` pass (nothing leaked into the
      sim path).
- [ ] Emscripten preset still configures/builds (memory growth is a few
      MB — sized as trivial, sanity check only).
- [ ] Smoke-launch of the installed binary per CLAUDE.md.

**Dependencies:** Tasks 1–4. **Scope:** M.

## Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Silent index-into-uint32 writes after widening (compiles fine, renders garbage) | High | Task 3 audit checklist; `test_blit` goldens; videotool frame diff |
| Reorder changes fade/rotation timing by one frame | Med | Phase 1 lands alone with identical-inputs argument + videotool diff before anything widens |
| Shadow/explosion semantics change where sprites overlap | Accepted | Isolated in Phase 2; side-by-side visual diff; documented |
| Video tool channel order / alpha regression | Low | Render-and-eyeball one replay; A=0xFF canonical |
| Emscripten memory growth | Low | Design doc sized it trivial; configure-check the preset |

## Open questions (deferred, not blockers)

- Whether `BLIT2` has any live use (delete if dead, found during Task 3).
- Whether the viewport/spectatorviewport duplication cleanup (flagged in
  the design doc as related-but-separate) is worth doing right after this
  lands, while the nine query sites are fresh.
