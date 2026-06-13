# Implementation Plan: Modern Colors — Stage 3 (Full-Fidelity Terrain)

**Status: implemented.** Task breakdown of Stage 3 of
`docs/ideas/modern-colors.md`: give TC authors true-color terrain art by
decoupling a level pixel's *material identity* (collision, destructibility,
shadow logic — stays 8-bit palette-indexed) from its *display colour* (a new
parallel ARGB layer that drives modern-mode renderers). Classic-mode
renderers, and any level without authored art, must stay **bit-identical to
today**.

Stages 1 and 2 are shipped (`modern-colors-stage1.md`,
`modern-colors-stage2.md`). Stage 2 deliberately installed the two seams
Stage 3 plugs into:

- `Level::AppearanceAt(idx, pal32)` (`level.hpp:37`) — the single indirection
  between renderer and level. Today returns `pal32[data[idx]]`; Stage 3 makes
  it mode-aware.
- `ShadowQuery` (`gfx/shadow_query.hpp`) — its header comment already names
  itself the "Stage 3 seam"; the call sites don't change, only `ShadowedArgb`'s
  body.

## Decisions taken before planning (confirmed)

1. **Authoring format: option (a), two-layer.** The level declares an optional
   ARGB display layer alongside the existing palette-indexed material layer.
   Absent → `display_data` is derived from the palette at load (visually
   identical to today). Existing TC packs load unmodified.
2. **Netplay ships in this stage** (not deferred to a v2). The display layer
   travels in the level blob; `kProtocolVersion` bumps. Rationale below.
3. **`Level::data` is renamed to `material_id`** tree-wide, per the design
   doc's framing. This is the larger-diff option (≈20 read/write sites) and is
   pure clarity — the field already *is* the sim source of truth — but it makes
   the material/display split legible at every call site and is done once here
   rather than left as debt.

## Corrections to the design doc (found during planning)

Grounding the plan in the current tree surfaced four places where reality
differs from `docs/ideas/modern-colors.md`'s Stage 3 section:

1. **"Replay format unaffected" / "netplay no change" is only true for
   classic levels.** For an *authored modern level*, the display layer must
   reach both the replay viewer and the netplay peer, because:
   - Replays embed the cereal'd `Game` → `Level` (`cereal_types.hpp:110-114`);
     a modern level's art lives nowhere else in the stream.
   - The netplay host sends **rasterized pixels**, not the level file
     (`session.cpp:695-719`); the peer reconstructs the level from that blob
     (`rollbackController.cpp:132-133`) and never re-loads from disk.
   So both the cereal `Level::serialize` and the wire blob grow the display
   layer, with a replay-version bump and a protocol bump. (Classic levels carry
   an empty layer and are byte-unaffected.)
2. **Display data is *not* simulation state.** It is never read by
   `processFrame`, never hashed (`stateHash.hpp:22,137`, `replay.cpp:252` all
   hash `data`/`material_id` only), so it cannot cause a desync. This is what
   makes omitting it from the wire merely *cosmetic* — and what lets the v1
   mutation policy (clear-on-hit) be non-deterministic-safe: `display_valid`
   clears are driven by the same deterministic mutations on every peer.
3. **In v1, `display_data` is immutable after load.** Runtime mutations only
   *clear* `display_valid`; they never write new ARGB. The snapshot therefore
   only strictly needs `display_valid`. We still snapshot both (design-doc
   faithful, oracle-symmetric, cost measured trivial) — see Task 5 note — but
   the immutability is why "clear-on-hit" needs no per-pixel colour bookkeeping.
4. **`AppearanceAt` needs the renderer's `mode`, which it currently lacks.**
   Stage 2 carries per-frame `pal32` on the `Bitmap` (decision #2 of the
   Stage 2 plan). The cheapest mode-aware `AppearanceAt` carries `mode` the
   same way — a new `Bitmap::mode` field set beside `pal32` each frame — so
   `DrawLevel`/`DrawMiniature` signatures stay stable.

## Architecture decisions

1. **Two new parallel arrays on `Level`,** beside the renamed `material_id`:
   ```cpp
   std::vector<uint8_t>   material_id;    // renamed from `data`; sim source of truth
   std::vector<Material>  materials;      // derived from material_id (unchanged)
   std::vector<uint32_t>  display_data;   // ARGB per pixel — authored modern art
   std::vector<uint8_t>   display_valid;  // 1 = use display_data; 0 = palette fallback
   ```
2. **Empty-when-classic.** For levels without an authored layer, `display_data`
   and `display_valid` stay **empty vectors**. `AppearanceAt` treats empty as
   "always fall back". Classic levels thus pay **exactly zero** extra memory,
   snapshot bandwidth, wire bytes, or replay bytes. The ~7 MB the design doc
   budgeted applies only when a modern level is actually loaded.
3. **Mode-aware `AppearanceAt`:**
   ```cpp
   uint32_t AppearanceAt(int idx, ColorMode mode, uint32_t const* pal32) const {
     if (mode == ColorMode::kModern && !display_valid.empty() && display_valid[idx])
       return display_data[idx];
     return pal32[material_id[idx]];
   }
   ```
   `Bitmap` gains `ColorMode mode`, set by the renderer beside `pal32`;
   `DrawLevel` passes `scr.mode`. Hot-toggle and per-window mode fall out for
   free — the branch is read per frame from the rendering renderer's state, no
   `display_data` rebake (design doc cross-cutting sections).
4. **v1 mutation policy: clear-on-hit.** Every level mutator clears
   `display_valid` for the cells it writes; hit terrain (holes, scorch, blood)
   falls back to palette-derived appearance. No ARGB sprite-source writes in v1
   (that is a v2 if a TC author wants painted scorch). `Level::SetPixel` clears
   `display_valid[idx]` in one place, covering the scorch/blood sites
   (`bobject.cpp:36,40,44`) for free.
5. **`ShadowedArgb` becomes display-aware.** In modern mode it returns a
   darkened sample of `display_data[wx,wy]` where valid, else the existing
   `pal32[material_id + 4]` path. `PixelAt`/`ShadowedIndex` (material queries)
   stay palette-indexed. Call sites unchanged.
6. **Snapshot both layers in both paths** (fast + cereal), gated on the layer
   being non-empty. Keeps the cereal correctness oracle symmetric with the
   fast ring (`test_rollback_correctness`). The "snapshot only `display_valid`"
   optimisation (valid because of decision-correction #3) is left as an
   available follow-up, not taken in v1.
7. **Format (a) loader.** The two-layer format carries, per level, an optional
   ARGB block. Concrete container shape is settled in Task 7 against the level
   loader (`Level::load`, `level.cpp:213-247`) and the TC archive reader; the
   guiding constraint is that a file without the block loads exactly as today.

## Interaction with powerlevels and palette-cycling animation

A "powerlevel" is just a level that ships its own 256-entry palette
(`POWERLEVEL` marker, `level.cpp:223-234`) into `level.origpal`. Animation is a
*separate* mechanism: `Palette::RotateFrom` (`palette.cpp:50-57`) cyclically
rotates a **range of palette indices** each frame — the hardcoded water shimmer
at indices 168–174 (`gfx.cpp:953`, `weapsel.cpp:22`,
`rollbackController.cpp:1235`) and the TC-defined ranges in `common->color_anim`
driven by `cycles>>3` (`game.cpp:173-175`). A level pixel's *index* never
changes; it animates purely because `pal32[index]` is rebuilt to different RGB
each frame.

**The conflict:** an authored pixel (`display_valid[idx]==1`) returns a fixed
`display_data[idx]` and **bypasses `pal32` entirely**, so any animated terrain
an artist paints over would freeze (paint over the water → it stops
shimmering). This is the live-terrain version of the frozen-capture animation
loss the Stage 2 plan already accepted.

**The resolution needs no new code — the per-pixel `display_valid` mask is the
opt-out.** Pixels the artist leaves unauthored keep `display_valid==0`, fall
through to `pal32[material_id]`, and animate exactly as today. So a modern level
ships static authored art for stable terrain (rock, dirt) and **leaves animated
ranges index-based**; the powerlevel's custom palette and `color_anim` ranges
keep driving those pixels. One level cleanly mixes static true-color art with
palette-cycled classic terrain. Classic powerlevels are wholly unaffected
(they never gain an authored layer → `display_valid` stays empty → always the
palette path, animation intact).

The only authoring constraint: the animated index ranges are fixed by the TC
(`common->color_anim` + the 168–174 shimmer), so "don't author over these
indices if you want them to keep moving" is a knowable, documentable rule
(settled in the Task 7 format spec).

Animated *true-color* terrain (cycling through authored ARGB colours rather than
VGA-palette ones) is **out of scope for v1** — it needs ARGB animation ramps in
the modern format, a real new mechanism. It is split out as its own stage in
`docs/ideas/modern-colors.md` (Stage 4 — Animated True-Color Terrain).

## Task list

### Phase 1: Rename + decouple (classic-identical, no behaviour change)

#### Task 1: Rename `Level::data` → `Level::material_id` tree-wide

**Description:** Mechanical rename of the field and every reader/writer.
Known sites (from a tree grep — confirm complete during the task):
`level.hpp` (field, `Pixel`/`Pixelp`/`SetPixel`/`CheckedPixelWrap`/`Swap`/
`AppearanceAt`/`DeriveHasCustomPalette` uses), `level.cpp` (`load`,
`GenerateDirtPattern`, `MakeShadow`, `Resize`, `DrawMiniature`),
`gfx/blit.cpp` (`DrawLevel` source ptr, `BlitImageOnMap`, `BlitStone`,
`DrawDirtEffect`, `CorrectShadow`), `gfx/shadow_query.hpp` (`PixelAt`),
`stateHash.hpp:22,137`, `replay.cpp:252`, `net/session.cpp:711`,
`controller/rollbackController.cpp:132-133`. Keep the cereal **nvp string**
`"data"` (binary archives ignore names; not touching it avoids any format
ambiguity) while renaming the C++ member it binds to. Optionally rename the
snapshot field `GameSnapshot::level_data` → `level_material_id` for
consistency (Task 5 owns the snapshot struct; decide there).

**Acceptance criteria:**
- [x] No `level.data` / `.data[` material accessor remains (grep audit).
- [x] Build green tree-wide incl. tests and tools.
- [x] `framehash` byte-identical to pre-rename on a classic replay; full
      determinism/rollback suites green (pure rename — zero behaviour change).

**Verification:** build + `ctest`; determinism suites; smoke-launch.
**Dependencies:** None. **Scope:** M (wide but mechanical).

#### Task 2: Add display layer to `Level`; mode-aware `AppearanceAt`; `Bitmap::mode`

**Description:** Add `display_data` / `display_valid` (empty by default;
`Swap` them; size them only when an authored layer is present). Rewrite
`AppearanceAt` to the mode-aware form. Add `ColorMode mode{kClassic}` to
`Bitmap` (`gfx/bitmap.hpp`); set it in the renderer beside `pal32`
(`gfx/renderer.cpp`). `DrawLevel` (`blit.cpp:147`) and `Level::DrawMiniature`
pass `scr.mode` into `AppearanceAt`.

**Acceptance criteria:**
- [x] Classic mode and empty-layer levels render byte-identical (`framehash`
      on a classic replay in both modes).
- [x] With a hand-built in-memory layer, modern mode returns `display_data`
      where valid and palette elsewhere; classic mode ignores the layer.
- [x] No allocation for classic levels (display vectors stay empty).

**Verification:** build + `ctest` incl. new `test_level_display`;
`framehash`; smoke-launch both modes. **Dependencies:** Task 1. **Scope:** M.

### Checkpoint A (after Tasks 1–2)
- [x] Tree renamed; display seam in place; output identical for all existing
      levels in both modes; new unit test exercises the synthetic layer.

### Phase 2: Mutation policy

#### Task 3: Mutators clear `display_valid` (v1 clear-on-hit)

**Description:** `Level::SetPixel` (both overloads) clears `display_valid[idx]`
when the layer is present — covers scorch/blood (`bobject.cpp:36,40,44`) for
free. Add the same clear to the direct level writers that bypass `SetPixel`:
`BlitImageOnMap`, `BlitStone`, `DrawDirtEffect`, `CorrectShadow`
(`gfx/blit.cpp`, via the `BLITL` macro region). No ARGB writes in v1.

**Acceptance criteria:**
- [x] Shooting a hole / splattering blood on an authored cell reverts that
      cell's `AppearanceAt` to palette in both modes (unit test).
- [x] Classic levels unaffected (clears are no-ops on empty layer).

**Verification:** build + `ctest`; `framehash`; smoke-launch.
**Dependencies:** Task 2. **Scope:** S.

### Phase 3: Persistence (shadow, snapshot, replay, netplay)

#### Task 4: `ShadowedArgb` display-aware

**Description:** Extend `ShadowQuery::ShadowedArgb` (`shadow_query.hpp:45-48`)
to sample a darkened `display_data[wx,wy]` in modern mode where valid (e.g.
halve each ARGB channel), else the existing `pal32[material_id + 4]` path.
`ShadowQuery` gains the rendering `mode` (it already holds `level`, `pal32`,
offsets). `PixelAt`/`ShadowedIndex` unchanged. Call sites in
`viewport.cpp`/`spectatorviewport.cpp` pass their renderer's `mode`.

**Acceptance criteria:**
- [x] Classic/empty-layer shadows byte-identical (`framehash`).
- [x] On an authored cell in modern mode, the shadow is a darkened display
      sample, not `pal32[idx+4]` (unit test on `ShadowQuery`).

**Verification:** build + `ctest` incl. `test_blit`/`test_level_display`;
`framehash`; smoke-launch. **Dependencies:** Task 2. **Scope:** S.

#### Task 5: Rollback snapshot — fast + cereal

**Description:** `GameSnapshot` (`fast_snapshot.hpp:159-160`) gains
`level_display_data` / `level_display_valid`; `Prepare` sizes them iff the
level has a layer (else leaves them empty). `SaveSnapshotFast` /
`LoadSnapshotFast` (`game.cpp:716-727,753-758`) memcpy the new arrays under
the same non-empty guard. Cereal `Level::serialize`
(`cereal_types.hpp:111-114`) serializes the display layer; `Game::load`
re-derives nothing for it (unlike `materials`) but tolerates an absent layer.
Bump `kMyReplayVersion` 7→8 with a pre-8 path that loads an empty layer.

**Acceptance criteria:**
- [x] `test_rollback_correctness` (cereal-vs-fast oracle) green with a
      synthetic modern level; `test_rollback_desync`, `test_determinism` green.
- [x] A pre-8 replay loads (empty layer, classic appearance) and plays
      identically.
- [x] Classic levels: snapshot/replay bytes unchanged (empty layer).

**Verification:** build + `ctest`; determinism/rollback suites; replay an old
`.lrp`. **Dependencies:** Tasks 2–3. **Scope:** M.

#### Task 6: Netplay level blob carries the display layer

**Description:** Extend the level blob (`session.cpp:695-719`) with the
display layer after the palette block; the peer reconstruction
(`rollbackController.cpp:132-133`) fills `display_data`/`display_valid`
(empty when absent). Bump `kProtocolVersion` (mismatched peers already refuse
each other). Display data is not hashed, so this is cosmetic-only and cannot
desync — but both peers must agree on it for modern levels to look right on
each screen.

**Acceptance criteria:**
- [x] `kProtocolVersion` bumped with a comment noting the added layer.
- [x] A modern level played in netplay renders authored art on **both**
      peers' modern-mode screens.
- [x] A classic level's blob is byte-identical in size to before (empty
      layer adds a zero-length section).
- [x] `test_rollback_*` / `test_determinism` green (nothing entered the hash).

**Verification:** build + `ctest`; two-instance local netplay smoke (or the
existing rollback/session tests where a live peer isn't available).
**Dependencies:** Task 5. **Scope:** M.

### Checkpoint B (after Tasks 4–6)
- [x] Display layer survives shadow rendering, snapshot, replay, and the wire;
      classic paths provably byte-unchanged; determinism suites green.

### Phase 4: Authoring + verification

#### Task 7: Modern level loader (format (a), two-layer)

**Description:** Define the optional ARGB block in the level container and
load it in `Level::load` (`level.cpp:213-247`): absent → derive `display_data`
from palette and leave `display_valid` empty (classic path); present → fill
both. Settle the concrete container shape against the TC archive reader here.
Ship one synthetic modern test level under `data/` for tests and manual checks.

**Acceptance criteria:**
- [x] An existing classic TC level loads with no layer and renders identical
      to today in both modes.
- [x] The synthetic modern level renders authored art in modern mode and
      palette-derived art in classic mode.
- [x] A level mixing authored static rock (`display_valid==1`) with
      **unauthored** water in the 168–174 range still shimmers in modern mode
      (the water stays index-based; only the authored rock is frozen ARGB).
- [x] The shipped format and its authoring rules are documented (Task 8).
- [x] `paths::ShadowsSystem` / Save-As guards still behave (no new shipped
      filenames collide).

**Verification:** build + `ctest`; load both levels; smoke-launch both modes.
**Dependencies:** Tasks 2, 5. **Scope:** L.

#### Task 8: Write the modern-level authoring guide

**Description:** Create `docs/modern-level-authoring.md`, a TC-author-facing
guide for the format settled in Task 7. It documents: the two-layer container
format (material layer + optional ARGB `display_data` block) and exactly what a
layer-absent file looks like (so existing packs keep working); the per-pixel
`display_valid` semantics (1 = authored ARGB, 0 = palette-derived fallback);
the **clear-on-hit** runtime behaviour (shot/scorched terrain reverts to
palette); and the **palette-cycling authoring rule** — which index ranges the
active TC animates (`common->color_anim` + the hardcoded 168–174 water shimmer)
and therefore which terrain to leave unauthored so it keeps moving in modern
mode. Include a worked example (static authored rock over palette-cycled water)
and a forward note that animated true-color terrain is Stage 4, which will
extend this same file. This is a Stage 3 deliverable, not an afterthought — it
is what makes the loader usable by anyone but its author.

**Acceptance criteria:**
- [x] `docs/modern-level-authoring.md` exists and matches the format Task 7
      actually shipped (container shape, `display_valid` semantics, clear-on-hit).
- [x] It lists the animated index ranges and the "leave unauthored to keep
      animating" rule, with a concrete mixed-terrain example.
- [x] It states that classic and existing TC packs load unchanged, and points
      forward to Stage 4 for animated true-color terrain.

**Verification:** doc review against the as-built loader (Task 7); links/paths
resolve.
**Dependencies:** Task 7. **Scope:** S.

#### Task 9: Full verification sweep (design doc Stage 3 checklist)

**Acceptance criteria (`modern-colors.md:516-526`):**
- [x] Classic levels render identical to today in classic **and** modern mode.
- [x] Modern level: authored art in modern, palette-derived in classic.
- [x] Palette-cycling (water shimmer / `color_anim`) still animates on
      unauthored terrain in modern mode; only authored cells are static.
- [x] Holes / blood fall back correctly in both modes (clear-on-hit v1).
- [x] Rollback tests pass with the larger snapshot.
- [x] Replay round-trips (pre-8 and v8); netplay shows art on both peers.
- [x] `double_res` mode renders the modern level correctly.
- [x] F10 hot-toggle still instant mid-game and in menus.
- [x] Emscripten preset still configures/builds (a few MB extra only when a
      modern level loads).
- [x] Smoke-launch of the **installed** binary per CLAUDE.md (ctest is not a
      smoke test).
- [x] `docs/modern-level-authoring.md` (Task 8) is present and accurate.

**Dependencies:** Tasks 1–8. **Scope:** M.

## Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Rename misses a `data` site → silent material/display confusion | Med | Task 1 grep audit; pure-rename `framehash` + determinism proof before any behaviour change |
| Stray display read bypasses `AppearanceAt` | Med | Audit the display readers (`DrawLevel`, `DrawMiniature`, `ShadowedArgb`); collision/hash readers correctly stay on `material_id` |
| Cereal/fast snapshot oracle mismatch on new fields | Med | Both paths grow the fields together; `test_rollback_correctness` is the oracle |
| Replay v7→v8 migration breaks old `.lrp` | Low | Explicit pre-8 path = empty layer; mirrors Stage 1's palette/worm migration |
| Protocol bump strands peers | Low/Expected | Existing version-mismatch refusal path; documented |
| Building ahead of real demand | Med (process) | Format/demand confirmed before this plan; synthetic level keeps the loader testable regardless |
| Memory / snapshot bloat | Low | Empty-when-classic gating: cost is exactly zero unless a modern level is loaded |

## Open questions (resolved)

- **Shadow darkening factor for `display_data`:** settled in Task 4 as
  `0xFF000000 | ((argb & 0x00FEFEFE) >> 1)` — 50% per-channel halve, alpha
  kept opaque. Reads close to the classic `pal32[idx+4]` look.
- **Container shape for the ARGB block:** settled in Task 7 as the `MODERNLV`
  8-byte magic block embedded at the end of the `.LEV` file, after any
  `POWERLEVEL` block. Layer-absent files load identically to today.
- **v2 painted mutations** (ARGB scorch/blood sprites writing `display_data`
  instead of clearing `display_valid`) — out of scope; clear-on-hit v1 shipped.
- **Animated true-color terrain** — out of scope; Stage 4. The `display_valid`
  mask already provides the extension point.
- **`material_id` rename of `GameSnapshot::level_data`** — decided in Task 5
  not to rename (cosmetic; `level_data` stays as-is in the snapshot struct).
