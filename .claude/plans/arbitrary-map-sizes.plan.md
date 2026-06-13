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

### PR 2 — New sized on-disk format + tools + 4096² test level
- **Format**: file beginning with a distinctive 8-byte magic (e.g. `OLLEVEL2`)
  + `version:u8` + `width:u16` + `height:u16`, then the **existing body layout**
  (material bytes → optional `POWERLEVEL` → optional `MODERNLV`…) so
  encoder/decoder code is shared. No magic ⇒ legacy 504×350 path
  (`level.cpp:213-344`). Enforce the **D2 4096² cap** with a clear error on load.
- **`load`**: replace `Resize(504, 350)` (`level.cpp:214`) with header-driven
  sizing on the new path.
- **Tools**: parametrize `tools/lev_gen.py` (`LEVEL_W/H`, line 26),
  `tools/lev_extract.py` (line 33), `tools/gen_stage4_anim.py` (lines 19-20) to
  read/emit the header and arbitrary sizes.
- **Test level (D5)**: `tools/gen_large_test.py` produces the 4096² modern level
  modeled on `modern_test.lev`, **generated on demand** (CMake/test fixture or
  documented one-liner) — not committed.
- **Doc**: update `docs/modern-level-authoring.md` — remove the fixed-504×350
  assumption, document the sized header, the 4096² max, and the larger-level
  workflow.
- **Mergeable because**: legacy files unchanged; random gen still 504×350; new
  files additionally supported.
- **Validate**: round-trip several sizes through gen/extract; load a stock legacy
  `.lev` and a new-format file; load the 4096² level and smoke-launch on it
  (exercises PR1's dynamic AI/stats); `test_level_display`; `test_paths`.

### PR 3 — Random map size in MATCH SETUP
- **Settings**: add `int32_t random_map_width{504}; int32_t random_map_height{350};`
  (`settings.hpp`), serialize in `SerializeSettingsScalars`
  (`cereal_types.hpp:189`), bump `kConfigVersion` **4 → 5** with comment.
- **Menu**: `kSiRandomMapWidth/Height` enum (`gfx.hpp`), `AddItem` near `kSiLevel`
  (`gfx.cpp:458`), `IntegerBehavior(common, …, 64, 4096, step)` cases in
  `GetItemBehavior`. Consider gating visibility on `random_level` (mirror
  `LevelSelectBehavior` interplay).
- **Generation**: feed the setting into `GenerateDirtPattern`/`GenerateRandom`
  (`level.cpp:12`) instead of hardcoded `Resize(504, 350)`.
- **Mergeable because**: defaults reproduce today's 504×350; older configs
  missing the fields fall back to defaults (existing v3-comment pattern).
- **Validate**: generate + smoke-launch a non-default random size; round-trip
  config; determinism suite.

### PR 4 — Zooming spectator viewport (render-to-scratch + downscale)
- Split `SpectatorViewport::Draw` (`spectatorviewport.cpp:33`) into a **world
  pass** (level + objects + worm sprites at 1:1 into a scratch bitmap sized to
  the visible region) and an **HUD overlay pass** (health bars/names/weapon
  lists/banner at native res on top, using the existing `kMultiplier` layout).
- Compute zoom each frame from both worms' bounding box (+ margin), clamp to map
  bounds and to **1× max** (never zoom in past native). Area-downscale scratch →
  spectator rect (extend `ScaleDraw`). Keep `Process()` clamp logic; floats are
  fine (display-only).
- Make HUD `stats_x`/`+68` offsets resolution-relative.
- **Mergeable because**: on 504×350 maps the bounding box fits, zoom stays 1×,
  output is visually unchanged.
- **Validate**: smoke-launch hotseat on the 4096² level, drive worms far apart,
  confirm both stay visible and HUD stays crisp at 1×.

### PR 5 — Configurable videotool output resolution
- **`tools_main.cpp`**: add `-w/-h` (or `--res WxH`) parsing alongside `-d/-s/-r`.
- **`replay_to_video.{hpp,cpp}`**: thread output dims through; remove hardcoded
  `kW=1280/kH=720` (lines 66-67); set spectator render resolution from the
  requested output.
- **`video_recorder.c:327`**: make `sws_getContext` input size dynamic (also
  fixes the latent 320×200 bug hardcoded to 640×400).
- **Mergeable because**: absent the flags it defaults to today's behavior.
- **Validate**: render a replay at 1280×720 and 1920×1080, spectator + normal
  mode; confirm output dimensions and no scaler mismatch.

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

## Acceptance

- [x] PR1: all 504×350 hardcodes read `level.width/height`; behavior unchanged; suites green.
- [ ] PR2: new sized format loads/saves; legacy files still load; 4096² level generates on demand; tools + doc updated.
- [ ] PR3: random map size editable in MATCH SETUP; config v5; defaults reproduce 504×350.
- [ ] PR4: spectator auto-zooms to keep both worms visible; 1× on small maps unchanged.
- [ ] PR5: videotool output resolution selectable; scaler input dynamic.
- [ ] Determinism/rollback suites + format checks pass on every PR.
