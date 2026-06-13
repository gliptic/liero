# Implementation Plan: Modern Colors — Stage 4 (Animated True-Color Terrain)

**Status: planned (not implemented).** Task breakdown of Stage 4 of
`docs/ideas/modern-colors.md`: let TC authors ship **authored terrain that
cycles colour in true colour** (water, lava, glowing crystals) — the
modern-mode equivalent of the classic `RotateFrom` palette animation. Stage 3
shipped *static* authored ARGB (`display_valid` / `display_data`) plus the rule
"leave animated terrain unauthored so it keeps palette-cycling." Stage 4 removes
that restriction by adding animation **to the authored layer itself**,
recomputed each frame from the existing simulation counter `cycles`.

Stage 3 is implemented (`docs/plans/modern-colors-stage3.md`) but **has not
shipped to any users**, so Stage 4 carries **no backward-compatibility burden
with the Stage 3 on-disk / wire / replay formats** — it is free to redefine
them. (Classic and Stage-1/2 replays, ≤ v7, with no display layer at all, are
real and must still load.)

## Hard constraints (inherited from Stages 1–3, verified against the tree)

- **Classic mode, and any level without ramps, render byte-identical to today**
  (Stage 3 static-authored levels included). The animated branch is never taken
  when `argb_ramps` is empty or `mode == kClassic`.
- **Zero new simulation state.** `AppearanceAt` / `ShadowedArgb` are render-only
  (callers: `blit.cpp:147` `DrawLevel`, `level.cpp:391` `DrawMiniature`,
  `shadow_query.hpp`); nothing Stage 4 touches enters `processFrame` or any
  state hash (`stateHash.hpp`, `replay.cpp` hash `material_id` only). Animated
  terrain therefore **cannot desync**.
- **Empty-when-absent.** A static (Stage 3) or classic level pays exactly zero
  extra memory / wire / replay bytes beyond a single `ramp_count = 0` byte.

## How Stage 3 left the seams (grounding)

Every extension point Stage 4 needs already exists:

| Seam | Current state | File |
|---|---|---|
| `AppearanceAt(idx, mode, pal32)` | mode-aware static branch | `level.hpp:37` |
| `Bitmap` per-frame render context | carries `pal32` + `mode`, set in `Renderer::UpdatePal32` | `bitmap.hpp:19-21`, `renderer.cpp:29` |
| `ShadowedArgb` | darkens static `display_data` in modern mode | `shadow_query.hpp:48-65` |
| Level loader | probes `POWERLEVEL` then `MODERNLV` magic blocks | `level.cpp:213-288` |
| Cereal `Level` (replay) | split save/load, `displayData`/`displayValid` gated `>= 8` | `cereal_types.hpp:119-135` |
| Wire blob | `has_display_layer` flag + payload appended after palette | `session.cpp:702-731`, `rollbackController.cpp:144-160` |
| Snapshot | `display_data`/`display_valid` (immutable in v1) | `fast_snapshot.hpp:159-163`, `game.cpp:728-775` |
| `cycles` | already sim state, already snapshotted | `game.hpp:131`, `fast_snapshot.hpp:143` |
| Shadow game | copies level layers at construction | `rollbackController.cpp:655-669` |

## Decisions taken during planning (confirmed)

1. **One block, not two.** The animation data extends the existing `MODERNLV`
   payload rather than appending a separate magic block. A second block only
   exists to let an old reader skip data it doesn't understand — with no shipped
   Stage 3 data that reason is gone, and a second block would just duplicate the
   probe/loader path and re-introduce the `POWERLEVEL`/`MODERNLV` byte-juggling
   (`level.cpp:266-272`).

2. **Animation data is never snapshotted.** The rollback ring exists to
   save/restore *mutable* simulation state. `argb_ramps`, `display_anim`, and
   the per-pixel phase offsets in `display_data` are **immutable level data
   loaded once**; the only level-display field that mutates at runtime is
   `display_valid` (clear-on-hit), and it is already snapshotted. So
   `GameSnapshot` gains **no new fields**. The anim data is established once per
   `Game` (disk load / wire reconstruction / shadow-game copy) and left untouched
   across every rollback. (Stage 3 already snapshots the immutable `display_data`
   "for oracle symmetry" — a ~700 KB/snapshot cost that is strictly unnecessary
   and could be dropped as a follow-up, out of scope here.)

3. **Menu previews render the phase-0 frame (`cycles = 0`).** `DrawMiniature`
   resolves animated pixels via `AppearanceAt`, which needs `cycles`. In-match
   minimaps have `game.cycles` in scope and animate; menu previews
   (`fileSelectorState.cpp:115`, `mainMenuState.cpp:133`) have no live `Game`, so
   they take the `Bitmap::cycles{0}` default — a static phase-0 frame. This
   matches how previews already behave (drawn once into `frozen_screen`, never
   re-animated).

4. **Version bumps without old-format readers.** The on-disk / wire / cereal
   formats genuinely change, so `kMyReplayVersion` bumps 8→9 and
   `kProtocolVersion` bumps 7→8 for honest format identity and the existing
   peer version-refusal. But there is **no pre-version read path** for the
   Stage 3 (v8 / protocol v7) format — dev-only Stage 3 replays/blobs are not
   supported. Real pre-display-layer replays (≤ v7) still load with an empty
   layer, exactly as today.

## Corrections to the design doc (found during planning)

Grounding the plan in the current tree and the "Stage 3 hasn't shipped" fact
surfaced two places where `docs/ideas/modern-colors.md`'s Stage 4 section is now
inaccurate:

1. **"With the same version bumps already paid there [Stage 3]" is wrong.** That
   assumed Stages 3 and 4 might ship together. Stage 3 shipped (to master) and
   locked replay v8 / protocol v7 **without any anim fields**, so Stage 4 must
   bump again (decision 4). This mirrors how the Stage 3 plan corrected the
   design's "replays/netplay unaffected" claim.
2. **The design implies the anim data might be snapshotted** ("the snapshot cost
   is identical to Stage 3's"). It needn't be snapshotted at all (decision 2);
   the cost is *lower* than Stage 3's, not equal.

## Robustness: untrusted level data must never crash or desync

The anim section arrives from disk (a hand-built `.lev`) **and over the wire from
a peer** (`rollbackController.cpp` level reconstruction), so it is untrusted
input. There are two distinct out-of-bounds risks:

1. **Parse-time overrun / DoS** — a crafted block claims a huge `color_count` or
   `ramp_count` and a naive loader reads or allocates past the buffer, before any
   pixel is drawn.
2. **Render-time OOB** — a `display_anim[idx]` names a ramp that doesn't exist, so
   `argb_ramps[a - 1]` reads off the end during `AppearanceAt`.

**Policy: reject-at-the-boundary + render-time backstop, degrading gracefully.**

- **At ingestion (loader *and* peer reconstruction):** validate before trusting
  any size — every `color_count` / `ramp_count` checked against the bytes
  actually remaining in the stream, with a sane cap on total ramp memory; every
  `display_anim` value checked `<= ramp_count`. On *any* inconsistency, **drop
  the whole anim layer to empty.** The level then plays as a static Stage-3 /
  palette level — no crash, no connection refusal.
- **At render:** the `a > argb_ramps.size()` and `colors.empty()` guards in
  `AppearanceAt` (architecture decision 2) are a memory-safety backstop, so even
  a validation bug cannot become an OOB read.

**Degrade, don't disconnect.** The anim layer is cosmetic and never enters the
state hash, so a peer whose anim layer is rejected stays in perfect simulation
lockstep — only the eye-candy differs per screen. There is no correctness reason
to refuse the match (unlike `material_id` / RNG state in the same blob, which are
sim-critical and already size-checked at `rollbackController.cpp:113-125`). A
malformed or hostile level therefore loses its animation and falls back to
static; it cannot crash the game or cause a desync.

## Architecture decisions

1. **New `Level` members** (empty by default, swapped in `Level::Swap`):
   ```cpp
   struct ArgbRamp {
     std::vector<uint32_t> colors;  // the cycle, in order (>= 1 entry)
     uint8_t shift = 0;             // phase = (cycles >> shift) % colors.size()
   };
   std::vector<ArgbRamp> argb_ramps;   // ramp table (empty => a static/Stage-3 level)
   std::vector<uint8_t>  display_anim; // per-pixel: 0 = static, N = ramp N-1
   ```
   For an animated pixel, `display_data[idx]` stores a **per-pixel phase offset**
   (so a wave can ripple across a surface), not a colour.

2. **`AppearanceAt` gains `int cycles`, with bounds-safety against malformed
   wire / replay / level data** (untrusted input — a peer or a hand-built `.lev`
   could carry a bad ramp index):
   ```cpp
   uint32_t AppearanceAt(int idx, ColorMode mode, uint32_t const* pal32,
                         int cycles) const {
     if (mode == ColorMode::kModern && !display_valid.empty() && display_valid[idx]) {
       uint8_t a = display_anim.empty() ? 0 : display_anim[idx];
       if (a == 0 || a > argb_ramps.size()) return display_data[idx];  // static / invalid
       ArgbRamp const& r = argb_ramps[a - 1];
       if (r.colors.empty()) return display_data[idx];
       unsigned phase = display_data[idx] + (static_cast<unsigned>(cycles) >> r.shift);
       return r.colors[phase % r.colors.size()];
     }
     return pal32[material_id[idx]];
   }
   ```
   All-integer and deterministic, but render-only — it never enters a hash.

3. **`cycles` reaches `AppearanceAt` carried on `Bitmap`** (same pattern as
   `mode`). `Bitmap` gains `int cycles{0}`, propagated in `Copy`, set at the
   gameplay draw sites where `game.cycles` is in scope (`viewport.cpp:205`,
   `spectatorviewport.cpp:187`, the in-game minimaps), defaulting to 0 for menu
   previews. `DrawLevel` (`blit.cpp:147`) and `DrawMiniature` (`level.cpp:391`)
   pass `scr.cycles`; their external signatures are unchanged.

4. **`ShadowedArgb` resolves the animated colour, then darkens.** `ShadowQuery`
   gains `int cycles`; for an authored animated cell it computes the same ramp
   colour as `AppearanceAt`, then applies the existing 50% halve
   (`0xFF000000 | ((argb & 0x00FEFEFE) >> 1)`). A single private resolve-helper
   on `Level` is shared by `AppearanceAt` and `ShadowedArgb` so the two cannot
   drift.

5. **File format: the `MODERNLV` payload is extended in place** (decision 1).
   New layout, appended after any `POWERLEVEL` block:
   ```
   magic         : 8 bytes    "MODERNLV"
   display_data  : cells * 4   ARGB32 LE      (phase offset where animated)
   display_valid : cells
   ramp_count    : 1 byte      (0..255)
   per ramp      : shift(1) + color_count(2 LE, >= 1) + colors(count * 4 ARGB32 LE)
   display_anim  : cells                       (present only when ramp_count > 0)
   ```
   A static modern level writes `ramp_count = 0` and omits `display_anim` — i.e.
   exactly today's Stage 3 file plus one zero byte. One magic, one loader, one
   validation path. The block is rejected (anim layer left empty) on any
   inconsistency (ramp count overruns the stream, a zero-length ramp, a
   `display_anim` value referencing a ramp `> ramp_count`).

6. **Anim data established once per `Game`, never snapshotted** (decision 2).
   Three establishment sites, mirroring `display_data`:
   - disk load (`Level::load`),
   - wire reconstruction (`rollbackController.cpp:147-160` peer preload — fill +
     validate from the new blob section),
   - shadow-game copy (`rollbackController.cpp:661-664` — copy `argb_ramps` /
     `display_anim` alongside the existing display-layer resize).

## Files to change

| File | Action | Why |
|---|---|---|
| `src/game/level.hpp` | UPDATE | `ArgbRamp`, `argb_ramps`, `display_anim`; `Swap`; `AppearanceAt(+cycles)`; resolve-helper |
| `src/game/level.cpp` | UPDATE | extended `MODERNLV` load + validation; `DrawMiniature` passes `scr.cycles` |
| `src/game/gfx/bitmap.hpp` | UPDATE | add `int cycles{0}`; `Copy` propagates it |
| `src/game/gfx/blit.cpp` | UPDATE | `DrawLevel` passes `scr.cycles` to `AppearanceAt` |
| `src/game/gfx/shadow_query.hpp` | UPDATE | `cycles` field; animation-aware `ShadowedArgb` |
| `src/game/viewport.cpp`, `spectatorviewport.cpp` | UPDATE | set `renderer.bmp.cycles` / `ShadowQuery.cycles = game.cycles` |
| `src/game/serialization/cereal_types.hpp` | UPDATE | `ArgbRamp` serialize; `Level` save/load anim fields gated `>= 9` |
| `src/game/version.hpp` | UPDATE | `kMyReplayVersion` 8→9 + comment |
| `src/game/net/transport.hpp` / `.cpp` | UPDATE | `kProtocolVersion` 7→8 + comment |
| `src/game/net/session.cpp` | UPDATE | append anim section to level blob |
| `src/game/controller/rollbackController.cpp` | UPDATE | peer reconstruction + shadow-game copy of anim data |
| `src/tests/test_level_display.cpp` | UPDATE | in-memory extended-`MODERNLV` round-trip; animated-`AppearanceAt` cases |
| `src/tests/test_blit.cpp` | UPDATE | animated `ShadowedArgb` case |
| `data/TC/openliero/Levels/modern_test.lev` | UPDATE | regenerate with one animated region (ramps + `display_anim`) |
| `docs/modern-level-authoring.md` | UPDATE | ramp-table + `display_anim` format, phase-offset semantics, animated example |

## Task list

### Phase 1 — Render path (no persistence; Stage-3-identical when no ramps)

#### Task 1: Level anim data model + animated `AppearanceAt` + `cycles` plumbing

**Description:** Add `ArgbRamp`, `argb_ramps`, `display_anim` to `Level` (empty
default; cleared in `load`; swapped in `Swap`). Add `int cycles{0}` to `Bitmap`
(propagated in `Copy`). Rewrite `AppearanceAt` to the bounds-safe animated form
(decision 2); add a private resolve-helper. `DrawLevel` / `DrawMiniature` pass
`scr.cycles`; set `renderer.bmp.cycles = game.cycles` at the gameplay draw sites.

**Acceptance criteria:**
- [ ] With no ramps, classic **and** modern render byte-identical to Stage 3
      (`framehash` on a classic level and on `modern_test.lev`, both modes).
- [ ] With a hand-built in-memory ramp, a marked pixel cycles through `colors`
      as `cycles` advances; the phase offset in `display_data[idx]` shifts the
      start; an out-of-range `display_anim` value falls back to static
      `display_data` (unit test).
- [ ] No allocation / no extra cost for classic & static-modern levels.

**Verification:** build + `ctest` incl. extended `test_level_display`;
`framehash`; smoke-launch. **Dependencies:** none. **Scope:** M.

#### Task 2: `ShadowedArgb` animation-aware

**Description:** `ShadowQuery` gains `int cycles`; `ShadowedArgb` resolves the
animated colour via the shared helper, then halves channels.
`viewport.cpp` / `spectatorviewport.cpp` set `.cycles = game.cycles` on their
per-frame `ShadowQuery`.

**Acceptance criteria:**
- [ ] Classic / no-ramp shadows byte-identical (`framehash`).
- [ ] Shadow under an animated authored cell tracks the cycle (darkened resolved
      colour, not a static `display_data` halve) — unit test in `test_blit.cpp`.

**Verification:** build + `ctest` incl. `test_blit`; `framehash`; smoke-launch.
**Dependencies:** Task 1. **Scope:** S.

#### Checkpoint A (after Tasks 1–2)
- [ ] Animation renders from `cycles` in modern mode; classic & static-modern
      output provably unchanged; unit tests exercise synthetic ramps. No
      persistence yet.

### Phase 2 — Persistence

#### Task 3: Extended `MODERNLV` loader + animated test level

**Description:** Load the extended `MODERNLV` payload in `Level::load`
(decision 5), with validation (reject → empty anim layer). Extend the in-memory
test helper in `test_level_display.cpp` to write the ramp table + `display_anim`;
assert round-trip. Regenerate `modern_test.lev` to include one animated region
(documented throwaway generator; mirrors how the Stage 3 file was produced).

**Acceptance criteria:**
- [ ] Classic `.lev` files and `ramp_count = 0` modern files load unchanged.
- [ ] `modern_test.lev` loads ramps + `display_anim`; the animated region cycles
      in modern mode, is static-or-palette in classic mode; its existing static
      authored pixels still behave per the Stage 3 tests.
- [ ] A malformed block (bad ramp count / zero-length ramp / out-of-range
      `display_anim`) loads as empty anim, no crash.
- [ ] `paths::ShadowsSystem` / Save-As guards unaffected (no shipped-name
      collision).

**Verification:** build + `ctest`; load the level; smoke-launch both modes.
**Dependencies:** Task 1. **Scope:** L.

#### Task 4: Cereal `Level` (replay) carries anim data; replay v9

**Description:** Add `ArgbRamp` cereal `serialize`; `Level` save always writes
`argb_ramps` / `display_anim`, load reads them when `g_cereal_replay_version >= 9`.
Bump `kMyReplayVersion` 8→9 with comment. No pre-9 read path (decision 4); ≤ v8
loads an empty anim layer.

**Acceptance criteria:**
- [ ] A pre-display-layer replay (≤ v7) loads with empty layers and plays
      identically.
- [ ] A v9 replay of `modern_test.lev` round-trips and re-renders the animation.
- [ ] Classic replay bytes unchanged in size (empty vectors).

**Verification:** build + `ctest`; replay an old `.lrp`; record + replay an
animated match. **Dependencies:** Task 3. **Scope:** M.

#### Task 5: Netplay level blob carries anim data; protocol v8; shadow-game copy

**Description:** Append an anim section to the level blob after `display_valid`
(`session.cpp`): `has_anim(1)` + `[ramp table + display_anim]`. Peer
reconstruction (`rollbackController.cpp:147-160`) fills + validates `argb_ramps` /
`display_anim` (empty when absent / invalid). Copy them to `shadowGame_->level`
at construction (`:661-664`). Bump `kProtocolVersion` 7→8 with comment.

**Acceptance criteria:**
- [ ] `kProtocolVersion` bumped; version-mismatch refusal still holds.
- [ ] An animated level played in netplay shows the cycling art on **both**
      peers' modern screens, in lockstep (shared `cycles`).
- [ ] A classic level's blob differs from before only by the new zero flag byte.
- [ ] `test_rollback_*` / `test_determinism` green (nothing entered the hash).

**Verification:** build + `ctest`; two-instance local netplay smoke (or the
session / rollback tests where a live peer isn't available).
**Dependencies:** Task 3. **Scope:** M.

#### Task 6: Snapshot / determinism confirmation

**Description:** Confirm `GameSnapshot` needs no new fields (decision 2). Ensure
the rollback oracle / desync / determinism tests build their test `Game` from an
animated level so the fast-restore game already carries the immutable anim data
(matching the cereal path).

**Acceptance criteria:**
- [ ] `test_rollback_correctness` (cereal-vs-fast oracle), `test_rollback_desync`,
      `test_determinism` green with the animated level.
- [ ] Shooting an animated cell clears `display_valid` → palette fallback in both
      modes (clear-on-hit unchanged); `display_anim` immutable.

**Verification:** the three determinism binaries (CLAUDE.md).
**Dependencies:** Tasks 4–5. **Scope:** S.

#### Checkpoint B (after Tasks 3–6)
- [ ] Animated terrain survives disk load, replay (v9), the wire (protocol v8),
      and rollback; classic & static-modern paths provably byte-unchanged;
      determinism suites green.

### Phase 3 — Docs + verification

#### Task 7: Update `docs/modern-level-authoring.md`

**Description:** Add the extended `MODERNLV` payload, the `ArgbRamp` table,
`display_anim` semantics, the **per-pixel phase-offset** meaning of
`display_data` for animated pixels, the clear-on-hit interaction (an animated
cell reverts to palette when shot), the version notes (replay v9 / protocol v8),
and a worked animated example (e.g. true-color water with `phase = distance from
shore`). Rewrite the existing "Stage 4 will add v2 mutations / animated terrain"
forward notes to describe the now-current state (no historical asides).

**Acceptance criteria:**
- [ ] Matches the as-built loader (payload shape, ramp encoding, `display_anim`).
- [ ] Includes the animated worked example and the phase-offset rule.
- [ ] No "deferred to Stage 4" forward notes remain for what this stage ships.

**Verification:** doc review against the as-built loader; links/paths resolve.
**Dependencies:** Task 3. **Scope:** S.

#### Task 8: Full verification sweep (design-doc Stage 4 checklist)

**Acceptance criteria (`modern-colors.md` Stage 4 verification):**
- [ ] A static (Stage 3) level with no ramps renders identically (animated branch
      never taken).
- [ ] An authored animated region cycles in modern mode; static-or-palette in
      classic mode.
- [ ] Two peers and a replay show the animation in lockstep (driven by `cycles`).
- [ ] Shooting animated terrain clears it to palette fallback (clear-on-hit).
- [ ] Shadows under animated terrain track the animation.
- [ ] Determinism / rollback suites pass (no new sim state; `cycles` already
      snapshotted).
- [ ] `double_res` mode and the F10 hot-toggle render the animated level
      correctly.
- [ ] Emscripten preset still configures.
- [ ] Smoke-launch of the **installed** binary per CLAUDE.md (ctest is not a
      smoke test).
- [ ] `docs/modern-level-authoring.md` (Task 7) is present and accurate.

**Dependencies:** Tasks 1–7. **Scope:** M.

## Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Malformed ramp index from wire / replay / `.lev` → OOB read | Med | Bounds-safe `AppearanceAt` (decision 2); loader / peer validation rejects to empty anim |
| `AppearanceAt` / `ShadowedArgb` drift on the ramp math | Low | Single shared resolve-helper on `Level` |
| Forgetting a `cycles` establishment site (peer / shadow) → peer art static while host animates | Med | Three explicit sites enumerated (decision 6); netplay two-peer acceptance check |
| Animation perceived as non-deterministic | Low | Render-only, recomputed from already-snapshotted `cycles`; never hashed → cannot desync (verified) |
| Regenerated `modern_test.lev` breaks Stage 3 tests | Low | Keep the existing static authored region intact; add the animated region beside it; re-run `test_level_display` |

## Open questions (resolved)

- **One block vs. two** — one (extended `MODERNLV`); no compat reason for a
  second block (decision 1).
- **Snapshot the anim data** — no; immutable, and variable-length ramps would
  break the no-alloc fast ring (decision 2).
- **Menu-preview `cycles`** — 0 (static phase-0 preview), the `Bitmap::cycles`
  default (decision 3).
- **Version bumps** — replay 8→9, protocol 7→8, with no Stage 3 (v8 / v7) read
  path (decision 4).
- **Test level** — one regenerated `modern_test.lev` with an animated region,
  plus an in-memory round-trip in `test_level_display.cpp`.
