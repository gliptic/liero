# Plan: Arbitrary Map Sizes + Zooming Spectator Viewport + Configurable Videotool Resolution

**Complexity**: Large (5 sequenced, independently-mergeable PRs)

## Summary

Today the game only supports 504×350 maps and the spectator window assumes the
whole map fits on screen. This plan adds: (1) arbitrary map sizes up to
4096×4096 via a new sized on-disk level format (legacy headerless 504×350 still
loads), (2) a spectator viewport that auto-zooms to keep both worms visible on
large maps, and (3) a selectable output resolution for the videotool. Work is
split into 5 PRs, each of which leaves master green and the game playable.

## Decisions

- **D1** — New magic-prefixed on-disk format for sized maps; legacy headerless
  504×350 files still load.
- **D2** — Maximum map size **4096×4096** (matches the existing net wire-transfer cap).
- **D3** — Spectator zoom via **render-to-scratch + area-downscale** (the world
  pass renders 1:1 into a scratch bitmap, then downscales into the spectator
  rect; HUD overlays draw on top at native resolution).
- **D4** — **Sequenced PRs, each independently mergeable.**
- **D5** — The 4096² test level is **generated on demand** (a tools script +
  CMake/test fixture), NOT committed to git (~117 MB raw).
- Random map-size option lives in **MATCH SETUP** (`SettingsMenu`).
- `docs/modern-level-authoring.md`, the `tools/` level scripts, and a new 4096²
  test-level generator all get updated.

## Key architecture findings

The **simulation core is already size-agnostic**: `Level` stores layers as
`std::vector`, `Resize(w,h)` is dynamic, and collision, spawning, blitting
(`gfx/blit.cpp`), state-hashing, cereal + fast snapshots, replay level capture,
and the network level transfer (already validates `0 < w,h ≤ 4096` and
`Resize`s) all read `level.width/height` dynamically. `Viewport` already takes
`(levwidth, levheight)`.

Blockers (hardcoded 504×350 or fixed-size structures):

| Blocker | Location | Issue |
|---|---|---|
| `Resize(504, 350)` | `level.cpp:214` (`load`), `level.cpp:12` (gen) | size baked in |
| Legacy `.lev` has no size header | `level.cpp:222` reads `w*h` before knowing size | can't encode other sizes on disk |
| Hardcoded viewport ctor | `localController.cpp:46-53`, `rollbackController.cpp:34-62`, `replayController.cpp:145-154`, `replay_to_video.cpp:60` | literal `504, 350` + `Rect(0,0,504+68,350)` |
| Fixed AI arrays | `ai/dijkstra.hpp:153-162` (`kFullWidth/Height`, `cells[]`), `ai/predictive_ai.hpp:230-237` (`CellState[17][12]`) | out-of-bounds / overflow on large maps |
| Fixed stats heatmaps | `stats_recorder.hpp:58-104` (`504/2,350/2,504,350`) | hardcoded dims |
| No world→buffer downscale | `spectatorviewport.cpp:33-38` maps world→buffer 1:1; `ScaleDraw` (`blit.cpp`) only integer-*upscales* | zoom-out is new capability |
| Videotool fixed dims | `replay_to_video.cpp:28-34,66-67` (640×400 render, 1280×720 out), `video_recorder.c:327` (sws scaler input hardcoded 640×400 — latent 320×200 bug) | no resolution selection |

**Determinism note:** rendering/zoom never runs in `processFrame`, and the
spectator view is local display only (not checksummed) — so zoom math may use
floats freely without touching the fixed-point determinism contract.

## Patterns to mirror (verified in-tree)

| Concern | Source | Pattern |
|---|---|---|
| Numeric match-setup setting | `gfx.cpp:1129` | `new IntegerBehavior(common, gfx.settings->field, min, max, step)` |
| Menu item registration | `gfx.cpp:457` + `gfx.hpp:72-98` enum | `AddItem(MenuItem(...))` + `kSi*` enum + `GetItemBehavior` case |
| Settings field + persistence | `settings.hpp:71-88`, `cereal_types.hpp:161-189` | field default + `make_nvp` + `kConfigVersion` bump (→ v5) |
| On-disk format extension | `level.cpp:230-332` | magic-probe blocks (`POWERLEVEL`/`MODERNLV`), `TryGet` tolerant reads |
| Downscale blit | `blit.cpp` `ScaleDraw` | existing integer-upscale routine to extend with downscale |
| Videotool CLI args | `tools_main.cpp:42-62` | `argv[i][1]` switch |

## PRs

### PR 1 — Foundation: make hardcoded 504×350 read `level.width/height` (pure refactor) — **DONE** ([#103](https://github.com/openliero/openliero/pull/103))
No behavior change; level still loads at 504×350. De-risks everything downstream.
- **Viewport construction**: replace literal `504, 350` in the controllers and
  `replay_to_video.cpp:60` with `level.width/height`; decouple spectator `Rect`
  from map size.
- **AI**: `ai/dijkstra.hpp:153-162` (`kFullWidth/Height` statics + fixed
  `cells[]`) → dynamic dims + heap buffer; `ai/predictive_ai.hpp:230-237`
  (`CellState[17][12]`) → dynamic grid.
- **Stats**: `stats_recorder.hpp:58-104` heatmaps → derive from `level.width/height`.
- **Mergeable because**: everything still runs at 504×350; identical behavior.
- **Validate**: full ctest + `test_determinism`/`test_rollback_*`; smoke-launch; clang-format/tidy diff.

### PR 2 — New sized on-disk format + tools + 4096² test level — **DONE**
- **Format**: `OLLEVEL2` 8-byte magic + `version:u8` + `width:u16 LE` + `height:u16 LE`
  (13-byte header total), then the existing body (material bytes → optional
  `POWERLEVEL` → optional `MODERNLV`). No magic ⇒ legacy 504×350 path.
  4096 cap enforced in loader, `lev_gen.py`, and `lev_extract.py`.
- **`Level::load`** (`level.cpp`): probes first 8 bytes via `TryGet`; on magic
  match reads 5-byte header and calls `Resize(w,h)`; otherwise prepends the
  probed bytes to the legacy material read (no seek on `io::Reader` needed).
- **Tools**:
  - `tools/lev_gen.py`: removed `--width`/`--height`; dims derived from `--mat`
    image; `open_and_check()` validates all other images match; OLLEVEL2 header
    written automatically for non-504×350 sizes.
  - `tools/lev_extract.py`: detects OLLEVEL2 magic; bounds-checks parsed dims;
    all MODERNLV parsing uses dynamic `cells = level_w * level_h`.
  - `tools/gen_stage4_anim.py`: deleted (outlived its usefulness).
  - `tools/gen_large_test.py` (new): generates a 4096×4096 OLLEVEL2 level with
    MODERNLV animated band; output gitignored; not committed.
- **Tests**: `src/tests/test_sized_level.cpp` — 12 Catch2 cases: legacy
  regression, sized load, material round-trip, 4096 cap, zero-dim rejection,
  MODERNLV at non-standard size, POWERLEVEL+MODERNLV combo, 1×1 boundary.
- **Doc**: `docs/modern-level-authoring.md` updated — canvas size 1×1–4096×4096;
  OLLEVEL2 header documented; lev_gen auto-dim detection; gen_large_test section;
  appendix with both file layouts side-by-side.

### PR 3 — Random map size in MATCH SETUP — **DONE**
- **Settings**: added `int32_t random_map_width{504}; int32_t random_map_height{350};`
  (`settings.hpp`), serialized in `SerializeSettingsScalars` (`cereal_types.hpp`),
  bumped `kConfigVersion` **4 → 5**.
- **Menu**: `kSiRandomMapWidth/Height` enum (`gfx.hpp`), items added near `kSiLevel`
  (`gfx.cpp`), `IntegerBehavior(common, …, 64, 4096, 8)` cases in `GetItemBehavior`,
  visibility gated on `gfx.settings->random_level` in `SettingsMenu::OnUpdate`.
- **Generation**: `GenerateRandom` calls `Resize(settings.random_map_width,
  settings.random_map_height)` instead of the hardcoded `Resize(504, 350)`.
  Added a `kMaxTries = width * height` retry cap to the rock-placement loops to
  prevent infinite spin on unusually small maps.
- **Bug fix**: `LevelSelectorState::OnSelected` now calls
  `gfx->settings_menu.UpdateItems()` after changing `random_level`, so MAP
  WIDTH/HEIGHT items appear immediately when switching to random in the same
  session (pre-existing gap, made visible by this PR).
- **Bug fix**: `Level` tracks `old_random_map_width/height` alongside the existing
  `old_random_level/old_level_file`; the new-game reuse condition in `gfx.cpp`
  now includes them, so changing the map size triggers regeneration without
  needing to toggle the REGENERATE LEVEL flag.
- **Tests**: `src/tests/test_random_map_size.cpp` — 8 Catch2 cases: default values,
  config version, TOML round-trip, backward-compat (missing keys fall back to
  504×350), and `GenerateFromSettings` at non-default and default dimensions.

### PR 4 — Zooming spectator viewport (render-to-scratch + downscale) — **DONE**

Delivered on branch `arbitrary-map-sizes-pr4`:

- `ScaleDrawArea` CPU box-filter downscale added to `gfx/blit.cpp`; 4 Catch2
  cases in `src/tests/test_blit.cpp`.
- `SpectatorViewport::Draw` split into world pass (scratch bitmap at visible
  world region, 1:1 pixel scale) + `ScaleDrawArea` composite + HUD overlay at
  native renderer resolution.
- `SpectatorViewport::Process` computes zoom from both worms' bounding box +
  60 px margin, clamped to 1.0× max (never zooms in past native pixels).
- Controller viewport rects changed to `Rect(0, 0, 640, 400)`; the old
  `504+68` centering hack removed.
- `DrawMiniature` gains independent `step_x`/`step_y` axes (two-axis overload,
  single-step wrapper kept for call-site compat) and is now `const`-qualified.
  All minimap draw sites updated; `fileSelectorState.cpp` adds `FillRect` before
  each draw to clear stale pixels when switching level previews.
- Spectator window renders at native pixel resolution: `OnWindowResize` queries
  `SDL_GetWindowSize` and calls `single_screen_renderer.SetRenderResolution(w,h)`;
  `SpectatorViewport` caches `render_w`/`render_h` from the renderer so
  `Process()` stays consistent without a `Renderer&` parameter. During
  single-screen replay `single_screen_renderer` is reset to 640×400 (it doubles
  as the main window primary renderer in that mode) and restored on game exit.

> **Known limitation**: extreme CPU slowdown when worms move far apart and
> zoom-out is triggered on a large native spectator window. Addressed in PR 7.

### PR 5 — Configurable videotool output resolution — **DONE**
- **`tools_main.cpp`**: add `-w/-h` (or `--res WxH`) parsing alongside `-d/-s/-r`.
- **`replay_to_video.{hpp,cpp}`**: thread output dims through; remove hardcoded
  `kW=1280/kH=720` (lines 66-67); set spectator render resolution from the
  requested output.
- **`video_recorder.c:327`**: make `sws_getContext` input size dynamic (also
  fixes the latent 320×200 bug hardcoded to 640×400).
- **Mergeable because**: absent the flags it defaults to today's behavior.
- **Validate**: render a replay at 1280×720 and 1920×1080, spectator + normal
  mode; confirm output dimensions and no scaler mismatch.

### PR 6 — Rollback snapshot optimization: dirty-cell tracking for large levels

Online play on large maps (e.g. 4096×4096) is slow because `SaveSnapshotFast`
memcpys ~48 MB per rollback frame (material_id + material flags + display_valid),
completely blowing out L3 cache every tick. Local play is unaffected (no rollback).

- **Dirty-cell bitset**: maintain a per-level bitset (or small sorted list) of cells
  modified since the last snapshot. Terrain destruction is already funneled through
  `Level::Modify` / the two `SetMaterial` helpers in `level.hpp` — add a dirty-mark
  there. Snapshot saves the full block on the first frame after `Prepare()`, then
  saves only dirty cells (index + value pairs) on subsequent frames.
- **Restore path**: restore full block from the last full snapshot, then apply
  the forward delta chain (or store snapshots as full + delta in alternation).
- **Alternatively**: store snapshots as always-full but skip `display_valid` cells
  that haven't been touched, using a dirty-bitset to know which 64-byte cache lines
  to copy. This keeps the restore path simple (one memcpy pass) at the cost of
  slightly more complex save logic.
- **`level.materials` deduplication**: `level.materials[i]` is always
  `common.materials[level.material_id[i]]`; after restoring `material_id` we can
  recompute `materials` in one O(W×H) pass and drop the 16 MB `level_materials`
  slot entirely.
- **Mergeable because**: snapshot is entirely in-memory; wire format and disk
  format are unaffected; existing rollback tests verify correctness.
- **Validate**: `test_rollback_correctness`, `test_rollback_desync`,
  `test_snapshot_fast` (add a dirty-tracking round-trip case); confirm online play
  on the 4096² level is no longer noticeably slower than local play.

### PR 7 — Rendering pipeline optimisation

With large maps and large native spectator windows, the rendering pipeline can
exceed the 14 ms frame budget in several places.  This PR opens with a profiling
pass to identify every hot path, then fixes them in priority order.  Any
findings that don't fit the scope (e.g. a major bottleneck in simulation,
networking, or palette handling) are noted as follow-on work rather than
stretching this PR.

#### Known bottleneck: spectator world pass

`SpectatorViewport::Draw` allocates a scratch bitmap sized to the visible world
region:

```
scratch_w = min(render_w / zoom, level.width)
scratch_h = min(render_h / zoom, level.height)
```

Every draw pass (level tiles, shadow pass, all sprite types) iterates over
every pixel of the scratch bitmap.  At a 1280×800 native window with zoom = 0.5
the scratch is 2560×1600 — 16× more pixels than the old fixed 640×400 case.
The subsequent `ScaleDrawArea` box-filter is also O(scratch_w × scratch_h), so
total cost scales as `(render_w × render_h) / zoom²`.

#### Tasks

0. **Profile first.**
   Run with a 4096² level and the spectator window at a large native size (e.g.
   1280×800), worms at maximum separation to force full zoom-out.  Use
   `perf record` / Instruments / Tracy (or even a manual frame timer around the
   key call sites) to confirm which functions actually dominate.  Let the profile
   guide which of the tasks below to tackle and in what order; skip tasks that
   don't appear in the hot path.  If the profiler surfaces a major bottleneck
   outside the rendering pipeline (simulation tick, palette rebuild, network
   processing) open a separate follow-on issue rather than folding it here.

1. **Cap the world-pass scratch at a fixed size and GPU-scale to the window.**
   Render the world pass into a bitmap capped at `min(render_w, level.width) ×
   min(render_h, level.height)` (i.e. never larger than the level itself,
   regardless of zoom).  Upload it to an SDL streaming texture and let
   `SDL_RenderTexture` with `SDL_SCALEMODE_LINEAR` (or `NEAREST` for the chunky
   pixel look) scale it to the native window on the GPU.  Draw the HUD overlay
   afterwards at native resolution via a second texture or directly into the
   renderer surface.  This decouples world-pass cost from native window size
   entirely and is expected to be the highest-ROI fix.

2. **Frustum-cull the sprite and shadow passes.**
   Currently every worm, wobject, nobject, bonus, and sobject is visited even
   when off-screen after zoom-out.  Add a cheap AABB check against the visible
   world rect `[x, x+scratch_w) × [y, y+scratch_h)` before each
   `BlitImage` / `BlitShadowImage` call.  On large maps with many off-screen
   objects this can cut the sprite-pass cost significantly.

3. **SIMD / vectorised `ScaleDrawArea`.**
   If the CPU downscale path is retained (e.g. for the videotool offline
   render), the inner accumulation loop in `ScaleDrawArea` (`blit.cpp`) is a
   natural auto-vectorisation candidate.  Only pursue this if tasks 1 and 2
   leave a measurable remainder in the profile.

4. **Cap minimum zoom.**
   A configurable floor (e.g. 0.25×) below which the viewport stops zooming out
   and pans to the midpoint instead gives a hard upper bound on scratch size
   independent of worm separation.  Expose as a hidden-menu option.  This is a
   UX trade-off rather than a pure optimisation, so treat it as optional and
   decide based on playtest feedback.

5. **Audit and fix any other hot paths found in step 0.**
   Document findings and fixes here as they are discovered.

#### Ordering

Do step 0 first.  Steps 1 and 2 are independent; step 1 gives the larger
speedup.  Steps 3 and 4 are conditional on profiling results.

- **Mergeable because**: purely display-side; does not touch simulation, net
  protocol, or on-disk format.
- **Validate**: confirm frame time is within budget at 1280×800 and 1920×1080
  with a 4096² level and worms at maximum separation.  Run `test_blit` to
  confirm `ScaleDrawArea` correctness is unchanged.  Smoke-launch +
  clang-format/tidy diff.

## Validation (every PR)

```bash
cmake --preset linux-x64 -DOPENLIERO_BUILD_TESTS=ON -DOPENLIERO_BUILD_VIDEOTOOL=ON
cmake --build build/linux-x64 --config Release
ctest --test-dir build/linux-x64 --build-config Release --output-on-failure
./build/linux-x64/Release/test_determinism && ./build/linux-x64/Release/test_rollback_correctness && ./build/linux-x64/Release/test_rollback_desync
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 8 ./install/linux-x64/bin/openliero
scripts/clang-format-diff.sh && scripts/clang-tidy-diff.sh build/linux-x64
```

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| 4096² test asset bloats git (~117 MB) | High | D5 — generate on demand |
| AI/stats overflow on large maps | High pre-PR1 | PR1 makes them dynamic before any large map can load |
| New magic collides with legacy material bytes | Low | 8-byte distinctive magic at file start; legacy path unchanged |
| Net play with mismatched sizes | Low | Wire transfer already sends + validates w/h |
| Memory at 4096² (~117 MB layers + AI) | Medium | D2 cap; fail loudly past 4096; document cost |
| Determinism regressions from PR1/PR3 sim touch | Medium | `test_determinism`/`test_rollback_*` on every PR |
| Rollback snapshot too large for online play on big maps | High (4096²) | PR6 — dirty-cell tracking; PR2 already fixed correctness |
| Spectator world-pass cost O((W×H)/zoom²) on large native windows | High | PR7 — GPU-scale world pass; frustum cull sprites |

## Acceptance

- [x] PR1: all 504×350 hardcodes read `level.width/height`; behavior unchanged; suites green.
- [x] PR2: new sized format loads/saves; legacy files still load; 4096² level generates on demand; tools + doc updated.
- [x] PR3: random map size editable in MATCH SETUP; config v5; defaults reproduce 504×350.
- [x] PR4: spectator auto-zooms to keep both worms visible; 1× on small maps unchanged; minimap scales correctly; weapon-select spectator view correct at all sizes; native-resolution spectator window.
- [x] PR5: videotool output resolution selectable; scaler input dynamic.
- [ ] PR6: online play on 4096² level no longer noticeably slower than local play; rollback tests green.
- [ ] PR7: spectator viewport frame time within budget at ≥1280×800 with worms at maximum separation on a 4096² level.
- [ ] Determinism/rollback suites + format checks pass on every PR.
