# Plan: Arbitrary Map Sizes + Zooming Spectator Viewport + Configurable Videotool Resolution

**Complexity**: Large (originally 5 sequenced PRs; PRs 1–7 landed, PR 8 code complete and in review — pending real-HW profiling)

## Summary

Adds: (1) arbitrary map sizes up to 4096×4096 via a new sized on-disk level
format (legacy headerless 504×350 still loads), (2) a spectator viewport that
auto-zooms to keep both worms visible on large maps, and (3) a selectable output
resolution for the videotool. PRs 1–7 are **merged to master**. **PR 8** pushes
the spectator frame time under the 14 ms budget on very large (4K-class) windows;
its code (Tasks 1 & 2) is **complete, tested, and committed** on branch
`spectator-render-resolution-cap` and open as a PR — the only remaining work is
real-hardware Tracy profiling + a visual playtest to close acceptance. See the
PR 8 section at the bottom; everything above it is background.

## Decisions (still load-bearing)

- **D2** — Maximum map size **4096×4096** (matches the net wire-transfer cap).
- **D3** — Spectator zoom via **render-to-scratch + downscale**: the world pass
  renders into a scratch bitmap, then is scaled into the spectator rect; HUD
  overlays draw on top.
- The simulation core is **size-agnostic** (`Level` stores layers as
  `std::vector`, `Resize(w,h)` is dynamic; collision, snapshots, net transfer,
  and `Viewport` all read `level.width/height`). Rendering/zoom never runs in
  `processFrame`, and the spectator view is local display only (not
  checksummed) — so **zoom math may use floats freely** without touching the
  fixed-point determinism contract.

## Status of merged work (PRs 1–7)

| PR | What shipped | Where the code lives |
|---|---|---|
| **PR 1** ([#103]) | Refactor: all hardcoded 504×350 read `level.width/height` (viewport ctors, AI arrays in `ai/dijkstra.hpp` + `ai/predictive_ai.hpp`, stats heatmaps in `stats_recorder.hpp`). No behavior change. | controllers, `ai/`, `stats_recorder.hpp` |
| **PR 2** | New `OLLEVEL2` sized on-disk format (8-byte magic + version + `width/height` u16 LE, then legacy body); legacy no-magic path unchanged. Tools (`lev_gen.py`, `lev_extract.py`, `gen_large_test.py`), 4096² generated-on-demand test level, `test_sized_level.cpp`, doc. | `level.cpp` (`Level::load`), `tools/`, `docs/modern-level-authoring.md` |
| **PR 3** | Random map size in MATCH SETUP (`random_map_width/height` settings, config v5, menu items, `GenerateRandom` resize). | `settings.hpp`, `cereal_types.hpp`, `gfx.{hpp,cpp}` |
| **PR 4** | Zooming spectator viewport: `ScaleDrawArea` CPU box-filter downscale; `SpectatorViewport::Draw` split into world pass → composite → HUD; zoom from worm bounding box; native-resolution spectator window. | `gfx/blit.{hpp,cpp}`, `spectatorviewport.{hpp,cpp}`, `gfx.cpp` |
| **PR 5** | Configurable videotool output resolution (`-w/-h`/`--res WxH`); dynamic sws scaler input. | `tools_main.cpp`, `replay_to_video.{hpp,cpp}`, `video_recorder.c` |
| **PR 6** ([#108]) | Rollback snapshot dirty-cell tracking so `SaveSnapshotFast` doesn't memcpy ~48 MB/frame on large maps. Dirty bitset in `Level`; `level.materials` dedup. Online-play-only. | `level.hpp` (`dirty_bits`/`dirty_list`), `fast_snapshot.hpp` |
| **PR 7** ([#114]) | Spectator rendering optimisation — **the direct predecessor to PR 8**. Detailed below. | `spectatorviewport.{hpp,cpp}`, `gfx.{hpp,cpp}`, `gfx/renderer.hpp` |

[#103]: https://github.com/openliero/openliero/pull/103
[#108]: https://github.com/openliero/openliero/pull/108
[#114]: https://github.com/openliero/openliero/pull/114

## PR 7 recap — the pipeline PR 8 builds on (merged, #114)

PR 7 replaced the CPU box-filter composite with a GPU scale and added a
downscaled world pass. The resulting **current** spectator present path (the
thing PR 8 optimises) is:

```
SpectatorViewport::Draw(game, single_screen_renderer, …)      spectatorviewport.cpp:102
  ├─ Fill(scratch_bmp, 0)                       full-scratch memset           :128
  ├─ World pass → scratch_bmp                                                 :130-649
  │     • zoom < 1  → DrawLevelScaled + BlitImageScaled at ~output res,
  │                    shadows/text/fire/laser/crosshair OMITTED (illegible)  :130-258
  │     • zoom >= 1 → 1:1 DrawLevel + shadow pass + sprite pass (byte-exact
  │                    legacy path; small maps unchanged)                     :259-649
  │     • all sprite/shadow loops are AABB frustum-culled (PR7 Task 2)
  ├─ GPU composite handoff (renderer.gpu_world_composite == true)             :658-675
  │     • stash scratch + used-rect + letterboxed dst-rect on the Renderer
  │     • FillTransparent(renderer.bmp)         full-overlay memset           :675
  └─ HUD overlay → renderer.bmp (native res, transparent bg)                  :704-842

Gfx::Flip()                                                   gfx.cpp:1116
  └─ if single_screen_renderer.gpu_world_src  → DrawSpectatorGpu(...)         :1128
        gpu_world_src is a strict ONE-SHOT (set per spectator draw, cleared
        every Flip) so menu/pause/direct-Flip frames fall back to CPU present.

Gfx::DrawSpectatorGpu(renderer)                               gfx.cpp:1065
  ├─ EnsureSpectatorWorldTexture(max_w, max_h)  grow-only STREAMING texture   :1067
  ├─ SDL_UpdateTexture(world, used_sub_rect, …) world DMA upload              :1078
  ├─ SDL_UpdateTexture(hud,   FULL window,   …) HUD overlay DMA upload        :1081
  ├─ SDL_SetTextureColorMod(both, fade)         reproduces ScaleDraw fade     :1089
  ├─ SDL_RenderClear (opaque black bars)                                      :1093
  ├─ SDL_RenderTexture(world, used → letterboxed dst, LINEAR)                 :1102
  ├─ SDL_RenderTexture(hud,   full → full)      BLEND over world              :1103
  ├─ SDL_RenderPresent                                                        :1104
  └─ SDL_SetTextureColorMod(both, 255)          restore neutral (bug 4f7a187) :1112
```

Relevant setup: the spectator renderer runs
`SDL_SetRenderLogicalPresentation(w, h, LETTERBOX)` with logical size **==**
window pixel size (`gfx.cpp` `OnWindowResize`, ~:405-414), so the
logical→physical transform is uniform and a manual letterboxed dst-rect lands
pixel-exact (proven by spike in PR7 Task 1a). `single_screen_renderer` renders
at the window size (`SetRenderResolution(w,h)`), and `render_res_x/y` on the
`Renderer` (`gfx/renderer.hpp:38-39`) is that size.

Two spectator-presentation bugs were fixed and are easy to re-break — keep them
in mind when touching this path:
- **`703af23`** — `gpu_world_src` made a strict one-shot (a stale handoff drove
  `DrawSpectatorGpu` against a resized menu layout → heap overread on pause).
- **`4f7a187`** — the fade `SDL_SetTextureColorMod` leaked onto the shared
  `sdl_spectator_texture`; the CPU present path never reset it → black spectator
  until resize. Always restore neutral colormod after a GPU present.

**PR 7 outcome:** spectator frame time improved **≈45 → ≈20 ms** (4096² level,
worms at max separation). Still **above the 14 ms budget at very large windows**
(≈20 ms at 3440×1440, max zoom-out). PR 8 closes that gap.

---

## PR 8 — Bound the spectator cost by a render-resolution cap (CODE DONE; PENDING REAL-HW PROFILE)

**Goal:** spectator frame time within the 14 ms budget at 4K-class windows
(3440×1440 and 3840×2160), 4096² level, worms at maximum separation (full
zoom-out — the worst case).

**Status (handover):** Tasks 1 and 2 are **implemented, tested, and committed**
on branch `spectator-render-resolution-cap` (full ctest 274/274, determinism +
rollback green, clang-format/clang-tidy clean, dummy-driver smoke clean). What
is **not** done and is the only thing between here and closing the acceptance
box: (a) the **real-hardware Tracy re-profile** confirming < 14 ms (cannot run
headless), and (b) a **visual spectator playtest** (the GPU present path is
guarded off under the dummy driver, so band correctness was proven by code
enumeration + a full-refresh safety net, not observed). Task 3 remains unstarted
and is only needed if (a) shows a remainder. One facet of Task 2 (native-res
crisp HUD) was deliberately deferred — see its section.

Commits: `ComputeCappedRenderResolution` helper + test, the cap wiring (Task 1),
`ComputeHudDirtyBands` helper + test, and the partial HUD clear/upload wiring
(Task 2).

### Problem analysis (profiling-grounded; re-confirm before/after on real HW)

After PR 7 every remaining cost in the spectator path **scales with the window
pixel count**, not the map. At 3440×1440 each full-window ARGB buffer is
~19.8 MB. Per spectator frame, zoomed out:

| Step | Code | Cost driver |
|---|---|---|
| `Fill(scratch_bmp, 0)` | `spectatorviewport.cpp:128` | full-window memset (~20 MB) |
| Downscaled world CPU draw | `spectatorviewport.cpp:130-258` | ~5 Mpx terrain + sprites |
| `FillTransparent(renderer.bmp)` | `spectatorviewport.cpp:675` | full-window memset (~20 MB) |
| `SDL_UpdateTexture(world, used)` | `gfx.cpp:1078` | ~20 MB CPU→GPU DMA |
| `SDL_UpdateTexture(hud, full)` | `gfx.cpp:1081` | ~20 MB CPU→GPU DMA |
| clear + 2× `RenderTexture` + present | `gfx.cpp:1092-1104` | GPU, cheap |

So per frame: **two full-window memsets + two full-window uploads** (~80 MB CPU
memory traffic + ~40 MB DMA), all sized to the *window*. At max zoom-out the
spectator shows the whole 4096² map shrunk into the window — the source detail
is already sub-pixel, so **there is no visual benefit to rendering or uploading
at full 4K.** The window resolution, not the map, is the cost.

### Key insight that makes the fix cheap

`SpectatorViewport::Process` (`spectatorviewport.cpp:51-100`) computes zoom from
the **worm bounding box**, and the visible world region is
`kViewW = render_w / zoom` (≈ bbox width + margin). Both `render_w` and `zoom`
scale together, so **the visible region is bbox-driven and independent of the
render resolution.** Lowering the spectator's internal render resolution changes
only the *sampling* resolution, not what is on screen. HUD coordinates are
already `render_res`-relative, so they lay out in the reduced space and upscale
consistently.

### Approach (in priority order)

#### Task 1 — Cap the spectator internal render resolution (primary fix, low risk) — ✅ DONE

**Shipped.** The spectator now renders at `min(window, cap)` preserving aspect,
and the existing `SDL_SetRenderLogicalPresentation` upscales the capped surface
to the physical window. This bounds all five window-sized costs (both memsets,
the world draw, both uploads) by the cap instead of the window.

- **Pure helper** `ComputeCappedRenderResolution(window_w, window_h, cap_h)`
  (`spectatorviewport.{hpp,cpp}`): caps height to `cap_h`, derives width from the
  window aspect; a no-op when `cap_h <= 0` (disabled) or `window_h <= cap_h`
  (so small windows are byte-for-byte unchanged); ≥1px width guard. Six
  `[spectator][rescap]` cases in `test_spectator_zoom.cpp`.
- **Applied** in `Gfx::OnWindowResize` (the spectator branch, ~`gfx.cpp:406`):
  the texture, draw surface, `SDL_SetRenderLogicalPresentation`, and
  `SetRenderResolution` all take the capped `kW/kH`. `render_res_x/y` then
  carries the capped size; world dst-rect math and HUD layout follow.
- **Setting** `max_spectator_render_height` (`AppSettings`, default **1080**,
  `0` = disabled): hidden-menu item "MAX SPECTATOR RES (H)"
  (`IntegerBehavior`, range 0–4320 step 120), cereal nvp
  `maxSpectatorRenderHeight`, `kConfigVersion` 5 → **6** (missing key → 1080).
  Takes effect on the next spectator-window resize / video-mode change.
- **Cost / tradeoff:** HUD text is upscaled with the world and goes slightly soft
  at the cap — Task 2's deferred facet (native-res HUD) would restore crispness.
- **Outstanding verify (real HW):** re-profile (Tracy zones already in place:
  `SpectatorViewport::Draw`, `Spectator::WorldPass::*`,
  `Spectator::Composite::GpuHandoff`, `Gfx::DrawSpectatorGpu`, `Gfx::Flip`) at
  3440×1440 and 3840×2160, max zoom-out; confirm the memsets/uploads shrank to
  the cap and a small map (zoom ≥ 1, window ≤ cap) is unchanged.

#### Task 2 — Partial HUD upload + partial clear — ✅ DONE (cost half); native-res HUD facet DEFERRED

**Shipped (the cost win).** The HUD overlay no longer clears/uploads the whole
window-sized buffer each frame; only the dirty bands.

- **Pure helper** `ComputeHudDirtyBands(render_h, banner_y, prev_banner_y)`
  (`spectatorviewport.{hpp,cpp}`): returns the full-width rows the HUD actually
  touches — bottom 40 px stats strip, the `y=164` reloading text, and a banner
  band that **unions current+previous frame** so a scrolling banner leaves no
  stale row — clamped to `[0, render_h)`, off-screen bands dropped. Band extents
  were derived by enumerating every `DrawBar`/`DrawString`/`FillRect` in the HUD
  block (glyph height 8, bars ≤5 px, all inside the 40 px strip; the holdazone/
  tag timer resolves to `render_h − 39`, also inside it) → coverage is provably
  complete. Four `[spectator][hudbands]` cases.
- **`FillTransparentBand(bmp, y, h)`** (`blit.{hpp,cpp}`) clears banded rows.
- **`SpectatorViewport::Draw`** clears only the bands (full clear on a resolution
  change) and hands them to the renderer (plain-int fields on `Renderer`);
  `prev_banner_y` + overlay dims tracked on the viewport.
- **`Gfx::DrawSpectatorGpu`** uploads only those bands. The first GPU frame after
  a resolution change *or any CPU present* (menu/pause/alloc-fail fallback)
  re-uploads the whole overlay, since the texture's untouched rows would
  otherwise be stale — guarded by `spectator_prev_present_gpu`, maintained in
  `Flip` and `DrawSpectatorGpu`. (This is the safety net that lets band
  correctness be asserted without a live GPU to observe.)
- **Effect:** per-frame HUD clear+upload drops from the full overlay (~16 MB at a
  1080 cap) to the bands (~1 MB), stacking on Task 1.

**Deferred facet — native-resolution crisp HUD.** The plan also framed Task 2 as
"keep the HUD overlay at *native* resolution" for crispness. That was **not**
done: with Task 1 the HUD renders at the capped resolution and is upscaled
(slightly soft). Making it crisp requires **decoupling the HUD's render
resolution from the world's** — the HUD block draws into `renderer.bmp` using
`renderer.render_res_y` / `render_w`, all currently the capped size, and
`single_screen_renderer` is shared with single-screen replay — so this is a
non-trivial refactor (two resolutions threaded through one Draw, or a separate
native-res HUD bitmap+texture). It is a **playtest-gated quality call** ("only if
the soft HUD is unacceptable"), so it waits on the real-HW playtest. If pursued:
keep the world capped, give the HUD its own native-res overlay bitmap/texture,
and recompute the bands in native-res coordinates.

#### Task 3 — Render the world pass into a locked texture (optional, only if still over budget)

`SDL_LockTexture` the world texture, render the **downscaled** pass straight into
its pixels, unlock — removes the scratch→texture memcpy *and* the scratch memset.
Medium risk: locked texture memory is typically write-combined (slow reads), so
this is only safe because the downscaled pass omits shadows (the read-modify-write
path). The 1:1 path (zoom ≥ 1, small maps) does RMW shadows and must **not** use
the locked path. Reach for this last.

### Explicitly deferred / not pursued

- **SIMD `ScaleDrawArea`** (old PR7 Task 3): irrelevant to the GPU spectator path
  — only the videotool/CPU composite (`spectatorviewport.cpp:676-702`) still uses
  `ScaleDrawArea`. Leave it for an offline-render perf pass if ever needed.
- **Minimum-zoom cap** (old PR7 Task 4): Task 1 already bounds cost, so a zoom
  floor is now a pure UX/visibility choice, not a performance need.
- **Dirty-rectangle sharing with rollback:** declined in PR 7 — `Level`'s dirty
  tracking is cumulative/never-cleared and terrain-only; sprites move every frame
  and aren't tracked. The GPU composite makes terrain caching moot.

### Mergeable because

Purely display-side: does not touch simulation, net protocol, or on-disk format.
The cap is the only new user-visible surface (a hidden-menu setting + one config
version bump).

## Validation (every PR)

```bash
cmake --preset linux-x64 -DOPENLIERO_BUILD_TESTS=ON -DOPENLIERO_BUILD_VIDEOTOOL=ON
cmake --build build/linux-x64 --config Release
ctest --test-dir build/linux-x64 --build-config Release --output-on-failure
./build/linux-x64/Release/test_determinism && ./build/linux-x64/Release/test_rollback_correctness && ./build/linux-x64/Release/test_rollback_desync
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 8 ./install/linux-x64/bin/openliero
scripts/clang-format-diff.sh && scripts/clang-tidy-diff.sh build/linux-x64
```

For PR 8, additionally re-profile with Tracy on real hardware at 3440×1440 and
3840×2160, 4096² level, worms at max separation, and confirm frame time < 14 ms.

## Risks (open)

| Risk | Likelihood | Mitigation |
|---|---|---|
| PR8 Task 1: capped-res HUD text too soft | Medium | **Open (playtest):** default cap 1080; native-res HUD facet of Task 2 deferred until playtest judges softness |
| PR8 Task 1: small-map / zoom≥1 path accidentally changed | Low | **Mitigated:** cap is a no-op when window ≤ cap; `[rescap]` test asserts identity; verify on real HW |
| PR8 Task 1/2: re-break the two PR7 present bugs (`703af23`, `4f7a187`) | Medium | **Code-mitigated, needs playtest:** `gpu_world_src` one-shot + neutral-colormod-restore untouched; Task 2 adds the `spectator_prev_present_gpu` full-refresh guard; dummy-driver smoke clean — but pause/menu visuals unobserved (no GPU here) |
| PR8 Task 2: dirty bands miss a HUD region → stale/ghost pixels | Medium | **Code-mitigated:** bands derived by exhaustive enumeration of the HUD draws + full-refresh on resize/CPU-present; **confirm by playtest** (death-banner scroll, pause round-trip) |
| PR8 Task 3 (not started): locked write-combined texture slow on RMW (zoom≥1) | Medium | Use locked path only for the shadow-omitting downscaled pass; only pursue if real-HW profile still over budget |
| Memory at 4096² (~117 MB layers + AI) | Medium | 4096 cap; fail loudly past it |

## Acceptance

- [x] PR1–PR6: see status table above (all merged).
- [x] PR7 ([#114]): spectator GPU composite + downscaled world pass + frustum
  cull; pause/start spectator bugs fixed; ≈45 → ≈20 ms. **Not yet within 14 ms**
  at very large windows.
- [x] **PR8 code:** Task 1 (render-resolution cap + hidden-menu setting, config
  v6) and Task 2 (partial HUD clear/upload) implemented with unit tests; full
  ctest 274/274, determinism/rollback green, clang-format/clang-tidy clean,
  dummy-driver smoke clean. Branch `spectator-render-resolution-cap`.
- [ ] **PR8 acceptance (real HW, blocks closing):** spectator frame time < 14 ms
  at 3440×1440 and 3840×2160, 4096², max zoom-out (Tracy re-profile); + visual
  playtest confirming no HUD ghosting (banner scroll, pause/menu round-trip) and
  unchanged small-map output. If still over budget → Task 3. If HUD too soft →
  native-res HUD facet of Task 2.
