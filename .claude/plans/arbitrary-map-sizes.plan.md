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

### PR 6 — Rollback snapshot optimization: dirty-cell tracking for large levels — **DONE** ([#108](https://github.com/openliero/openliero/pull/108))

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

#### Profiling result (2026-06-15, 4096² level, 1280×800 spectator, worms apart)

Tracy zones confirm the cost split inside `SpectatorViewport::Draw` (~45 ms):

| Stage | Code | Cost |
|---|---|---|
| World pass → `scratch_bmp` (DrawLevel + shadows + sprites, 1:1) | `spectatorviewport.cpp:94-489` | DrawLevel **6 ms** + sprites |
| **`ScaleDrawArea` CPU box-filter** scratch → `single_screen_renderer.bmp` (native res) | `spectatorviewport.cpp:521`, impl `blit.cpp:770-799` | **~38 ms** ← dominant |
| `ScaleDraw` integer-magnify in `Gfx::Draw` (kMag=1 at native ⇒ plain copy) | `gfx.cpp:1013` | redundant |
| `SDL_UpdateTexture`/`SDL_RenderTexture`/present | `gfx.cpp:1016-1019` | already GPU |

`ScaleDrawArea` reads **every** source pixel with a per-output-pixel divide; at
zoom 0.5 the source is 2560×1600 = 4M px/frame. Cost scales as
`(render_w × render_h) / zoom²`. **The CPU composite — not the world drawing — is
the bottleneck.** The world pass itself (DrawLevel 6 ms + sprites) is comparatively
cheap, so shrinking or caching it is low-ROI; eliminating the composite is the win.

#### Current presentation pipeline (the code being restructured)

The spectator window goes through **two** CPU scaling passes today. Dataflow:

```
SpectatorViewport::Draw(game, single_screen_renderer, ...)   spectatorviewport.cpp:82
  ├─ world pass → scratch_bmp           1:1 world pixels, size capped to level
  │                                      (spectatorviewport.cpp:94-489)
  ├─ Composite: scratch_bmp ─ScaleDrawArea(box filter)→ single_screen_renderer.bmp
  │                                      (spectatorviewport.cpp:491-524)  ← ~38 ms
  │     • memcpy fast path when scratch dims == output dims (510-519)
  │     • ScaleDrawArea otherwise (521)
  │     • Fill(renderer.bmp, 0) clears letterbox bars OPAQUE black (503-505)
  └─ HUD: drawn directly into single_screen_renderer.bmp at native res (526-664)

Gfx::Flip()                                                   gfx.cpp:1024
  ├─ Draw(*sdl_draw_surface, *sdl_texture, *sdl_renderer, *primary_renderer)        // main window
  └─ Draw(*sdl_spectator_draw_surface, *sdl_spectator_texture,
          *sdl_spectator_renderer, single_screen_renderer)    // spectator window (1030-1033)

Gfx::Draw(surface, texture, sdl_renderer, renderer)           gfx.cpp:983
  ├─ ScaleDraw(renderer.bmp → surface, kMag)  integer magnify; kMag==1 at native ⇒ plain copy (1013)
  ├─ SDL_UpdateTexture(texture, surface.pixels, ...)          (1016)
  ├─ SDL_RenderClear / SDL_RenderTexture(texture, NULL, NULL) (1017-1018)
  └─ SDL_RenderPresent                                        (1019)
```

Relevant SDL setup (`Gfx::OnWindowResize`, gfx.cpp:390-414): the spectator texture
`sdl_spectator_texture` is `SDL_TEXTUREACCESS_STREAMING` sized to the **window**
(w,h); `sdl_spectator_draw_surface` is a matching ARGB8888 surface; and
`SDL_SetRenderLogicalPresentation(sdl_spectator_renderer, w, h, LETTERBOX)` is
already applied (gfx.cpp:407). `single_screen_renderer.bmp` is sized to the window
via `SetRenderResolution(w,h)` (gfx.cpp:412).

So today both the world downscale (`ScaleDrawArea`) and a redundant native-res copy
(`ScaleDraw`, kMag=1) run on the CPU before the single GPU upload+present.

#### Decision: GPU-scale the composite, keep the 1:1 world pass

The earlier "cap scratch to render resolution" attempt was reverted (`8e1b028`,
PR7 Task 1a) because the world pass blits at 1:1 — capping the scratch below the
visible region merely clips the view. The corrected approach keeps the 1:1 world
pass (its drawing cost was never the problem) and replaces the **CPU composite**
with a **GPU scale**:

1. World pass still renders into `scratch_bmp` at 1:1 (unchanged).
2. Upload `scratch_bmp` to a dedicated SDL **streaming texture**, then
   `SDL_RenderTexture` it to the letterboxed dest rect on the GPU
   (`SDL_SCALEMODE_LINEAR`). This **deletes the ~38 ms `ScaleDrawArea` and the
   redundant `ScaleDraw` copy**, replacing them with a texture upload (~16 MB DMA,
   a few ms) plus a sub-ms GPU blit.
3. HUD renders into a native-res layer with a **transparent background** → overlay
   texture → second `SDL_RenderTexture` on top with alpha blend.

To avoid recreating a texture every frame as zoom (and thus scratch size) changes,
allocate the world texture **once at max size** (level dims clamped to a ceiling),
upload only the used sub-rect, and pass that sub-rect as `SDL_RenderTexture`'s
srcrect.

`ScaleDrawArea` **stays** — `video_tool` renders offline with no GPU/window and
still needs the CPU path. The GPU path is **spectator-window-only**, guarded on a
live SDL renderer (must not crash under the dummy driver in CI smoke).

#### Dirty-rectangle sharing — evaluated, declined

Considered sharing the rollback dirty-tracking to lessen rendering. Finding:
there is **no dirty-rect system in `RollbackController`**; the dirty tracking
lives in **`Level`** (`level.hpp:94-106,199-200`: `dirty_bits` + `dirty_list`)
and feeds `SaveSnapshotFast` (PR6). It is unsuitable for rendering reuse:

- It is **cumulative and never cleared** (`level.hpp:197-198`) — it only grows;
  rendering needs per-frame dirty regions.
- It tracks **terrain cells only**. But terrain draw (`DrawLevel`) is just 6 ms,
  not the bottleneck; sprites/shadows/worms move every frame and aren't tracked.

At best it could cache the terrain layer (~6 ms) at real complexity cost, and the
GPU composite makes that moot. **Not pursued.**

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

1. **GPU-scale the world composite (keep the 1:1 world pass).** ← see Decision above
   Do NOT cap the scratch below the visible region (that was Task 1a, reverted in
   `8e1b028` — it clips the view). Highest-ROI fix (removes the ~38 ms composite +
   redundant copy). Files: `spectatorviewport.{hpp,cpp}`, `gfx.{hpp,cpp}`
   (`Draw`/`Flip`/`OnWindowResize` + new texture members in `gfx.hpp`),
   `src/tests/test_spectator_zoom.cpp`. Sub-steps:

   - **1a. Spike the SDL scaling path first. — RESOLVED (2026-06-15): keep logical
     presentation; manual dest rect cooperates.** The spectator renderer runs
     `SDL_SetRenderLogicalPresentation(w,h,LETTERBOX)` (gfx.cpp:407). The question was
     whether a manual letterboxed `SDL_RenderTexture` dest rect cooperates with that
     or whether logical presentation must be dropped.
     **Finding (empirical):** an SDL3 software-renderer spike replicating the
     spectator setup (logical size == window pixel size, LETTERBOX) blitted a world
     texture into a manually-computed centred dest rect and read pixels back: the
     readback framebuffer is the logical size (1280×800), the world colour lands
     pixel-exactly inside the manual dest rect, and `SDL_RenderClear` produces exact
     black bars outside it. Because the logical size **equals** the window pixel size,
     the logical→physical transform is uniform (identity modulo HiDPI), so there is
     **no double-letterboxing**.
     **Decision for 1b/1c:** keep `SDL_SetRenderLogicalPresentation(w,h,LETTERBOX)`
     untouched; render coordinates are then in logical == window pixels. Reuse the
     existing CPU-composite aspect math (`kOutX/kOutY/kOutW/kOutH`,
     spectatorviewport.cpp:496-501) directly as the `SDL_RenderTexture` dstrect (as
     `SDL_FRect`); `SDL_RenderClear` the spectator renderer to opaque black for the
     bars before the world blit. No `FitScreen`-style manual letterbox is needed.
     The "allocate world texture once at max size, upload only the used sub-rect, pass
     that sub-rect as `srcrect`" approach (1b) is orthogonal to logical presentation
     (standard `SDL_RenderTexture` srcrect semantics) and is low-risk — not separately
     spiked. (Spike source kept out-of-tree under `/tmp/sdl_spike_1a.cpp`; not committed.)
   - **1b. World texture. — DONE (2026-06-15, `cc0ae79`).** Added
     `sdl_spectator_world_texture` (STREAMING, ARGB8888), allocated **once per
     level** at the level size clamped to `kSpectatorWorldTextureMax` (4096) via
     the pure, unit-tested `SpectatorWorldTextureSize`. Implemented as a lazy
     `EnsureSpectatorWorldTexture(need_w, need_h)` (grows only; no per-frame
     realloc) rather than in `OnWindowResize`, because the world texture is
     level-sized and window-resize-independent (the spectator renderer persists
     across window resizes; only `SetVideoMode`'s renderer-destroy invalidates
     it, where it's nulled for lazy rebuild). Each frame `SDL_UpdateTexture`
     uploads only the used `kScrW×kScrH` sub-rect, then `Gfx::DrawSpectatorGpu`
     `SDL_RenderTexture`s it (sub-rect srcrect → letterboxed dstrect from the
     shared `ComputeSpectatorDstRect`, `SDL_SCALEMODE_LINEAR`). Replaced the
     `ScaleDrawArea`/memcpy composite block in `SpectatorViewport::Draw`.
   - **1c. HUD overlay. — DONE (2026-06-15, `cc0ae79`).** In the GPU path the
     world-pass clear of `renderer.bmp` switched from `Fill(renderer.bmp, 0)`
     (opaque `pal32[0]`) to the new `FillTransparent` (ARGB `0x00000000`); the
     HUD draws into that same native-res `single_screen_renderer.bmp` (now
     HUD-only) and uploads to the existing `sdl_spectator_texture` (now
     `SDL_BLENDMODE_BLEND`), `SDL_RenderTexture`d on top of the world. Bars/text
     helpers write opaque `pal32` ARGB and keep working. `ScaleDraw`'s
     per-channel fade is reproduced via `SDL_SetTextureColorMod` on both layers
     so the spectator fade-in survives.
   - **1d. Scope the GPU path correctly. — DONE (2026-06-15, `cc0ae79`).**
     `Gfx::SpectatorGpuComposite()` enables the path only when `spectator_window`
     is set, a live `sdl_spectator_renderer`/`sdl_spectator_texture` exist, and
     `primary_renderer != &single_screen_renderer` (so single-screen replay to
     the main window keeps the CPU `Gfx::Draw` path). The flag is set per frame
     before each spectator-viewport draw and `gpu_world_src` reset, so a frame
     that doesn't redraw the viewport (menu over a frozen game) falls back to the
     CPU present. The dummy driver / videotool never satisfy the guard. Headless
     smoke (dummy driver) launches clean.
   - **1e. Retain `ScaleDrawArea`. — DONE (kept intact).** The CPU composite
     branch (videotool / single-screen replay) still box-filters the scratch into
     `bmp` via `ScaleDrawArea`; `test_blit` unchanged (83 assertions, green).
     Note: videotool is off by default (needs system ffmpeg) so it wasn't
     rebuilt; its `ScaleDrawArea` caller is untouched.
   - **1f. Profile to confirm. — DONE (2026-06-15, on real hardware).** Tracy on
     the 4096² level, worms at max separation:
     - GPU composite alone (`cc0ae79`): the ~38 ms CPU composite zone dropped to
       ~0, but the 1:1 world pass + 64 MB texture upload remained → **~34 ms**
       total at 1280×800 (`local:draw` 21 ms incl. `DrawLevel` 5.9 ms; `Gfx::Flip`
       11 ms incl. the upload). Over budget.
     - Downscaled world pass (`e0a0d6c`, see 1g): **~20 ms** at 3440×1440 (4.95 Mpx
       window) at max zoom-out. The world pass and its upload are now bounded by
       the window, not the level; the residual is dominated by the two
       full-window texture uploads (world + HUD overlay) + present, which scale
       with window pixels.
     - **Outcome:** a large improvement (≈34→20 ms) but **still above the 14 ms
       budget at very large windows**. Accepted for now per playtest. Further
       gains would come from shrinking/scoping the per-frame uploads (HUD
       dirty-region upload, or capping the spectator render resolution
       independently of window size) — tracked as a follow-on, not blocking.
   - **1g. Downscaled world pass. — DONE (2026-06-15, `e0a0d6c`).** Render the
     world pass at ~output resolution when `zoom < 1` instead of 1:1, so its CPU
     cost and texture upload are bounded by the window rather than the level
     area. `ComputeWorldPassScratch` (unit-tested) gives `scale = min(1, zoom)`
     and a scratch ≤ the render surface; new `DrawLevelScaled` / `BlitImageScaled`
     (nearest-neighbour) render terrain and sprites scaled down. `zoom ≥ 1` keeps
     the existing 1:1 path byte-for-byte (no change for small maps). The
     downscaled path draws the visually-significant world (terrain, worms +
     ninjarope, bonuses, s/w/n-objects, blood) and **omits** detail that is
     sub-pixel/illegible at that zoom (shadows, text labels, fire cones, laser
     sight, aim crosshair, AI debug). The world texture is now window-bounded, so
     `SpectatorWorldTextureSize`/the 4096 ceiling were removed.
   - **1h. Spectator presentation bug fixes (found on real hardware).**
     - **Pause crash / blank (`703af23`).** `MainMenuState::Enter` calls `Flip()`
       directly (outside `RunOneFrame`), so a stale `gpu_world_src` from the last
       gameplay frame drove `DrawSpectatorGpu` against the just-resized menu
       layout → heap overread. Fixed by making `gpu_world_src` a strict one-shot
       (gated on, and cleared by, every Flip); non-gameplay frames fall back to
       the CPU present.
     - **Black spectator at start / pause until resize (`4f7a187`).** The GPU
       fade `SDL_SetTextureColorMod` leaked onto the shared `sdl_spectator_texture`
       (at fade 0 → multiply-by-black); the CPU present path never reset it.
       Fixed by restoring neutral colormod after each GPU present.

2. **Frustum-cull the sprite and shadow passes.** — **DONE** (`bd8cb77`, PR7 Task 2)
   Every worm, wobject, nobject, bonus, and sobject now gets a cheap AABB check
   against the visible world rect `[x, x+scratch_w) × [y, y+scratch_h)` before each
   `BlitImage` / `BlitShadowImage`.

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
| PR7-1a: `SDL_SetRenderLogicalPresentation(LETTERBOX)` conflicts with manual dest-rect scaling | ~~Medium~~ **Resolved** | Spike (Task 1a, 2026-06-15) proved no conflict: logical size == window pixels ⇒ uniform transform, manual dest rect lands pixel-exact. Keep logical presentation. |
| PR7-1b: per-frame world-texture upload (≤16 MB+, grows with scratch) becomes the new cost | Medium | Still ≪ 38 ms; cap world-texture max size; measure in Task 1f |
| PR7-1c: HUD transparency/blend regressions (today's clear is opaque black) | Low | Clear HUD buffer to alpha 0; visual check; HUD is small |
| PR7-1d: GPU path crashes/no-ops under dummy driver (CI smoke) or in single-screen-replay primary-renderer mode | Medium | Guard on live spectator renderer/window; keep `Gfx::Draw` CPU path for those modes |

## Acceptance

- [x] PR1: all 504×350 hardcodes read `level.width/height`; behavior unchanged; suites green.
- [x] PR2: new sized format loads/saves; legacy files still load; 4096² level generates on demand; tools + doc updated.
- [x] PR3: random map size editable in MATCH SETUP; config v5; defaults reproduce 504×350.
- [x] PR4: spectator auto-zooms to keep both worms visible; 1× on small maps unchanged; minimap scales correctly; weapon-select spectator view correct at all sizes; native-resolution spectator window.
- [x] PR5: videotool output resolution selectable; scaler input dynamic.
- [x] PR6: online play on 4096² level no longer noticeably slower than local play; rollback tests green.
- [~] PR7: spectator frame time greatly improved (≈34→20 ms) via GPU composite +
  downscaled world pass; pause/start spectator bugs fixed. **Not strictly within
  the 14 ms budget** at very large windows (≈20 ms at 3440×1440, max zoom-out) —
  the residual is the two full-window texture uploads, accepted for now. Frustum
  cull (Task 2) done; Tasks 3/4 (SIMD, min-zoom cap) left as optional follow-ons.
- [ ] Determinism/rollback suites + format checks pass on every PR.
