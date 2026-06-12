# Implementation Plan: Modern Colors — Stage 1 (Modern Player Colors)

**Status: complete** (branch `modern-colors-stage1`). All six tasks landed;
checkboxes below reflect the final state. Two checks remain manual-only:
a visual A/B of the toggle in a real window, and an actual old-vs-new
client handshake refusal (the version-check path is unit-tested).

Task breakdown of Stage 1 of `docs/ideas/modern-colors.md`, plus the
cross-cutting hot-toggle / per-window / toggle-UX work the design doc ships
with Stage 1.

## Outcome — deviations from the plan as written

- **Replays embed the cereal'd `Game`** (level palette + worm settings),
  which the plan missed: `kMyReplayVersion` bumped 6→7 with on-load
  expansion of pre-7 palettes and worm rgb (including the mid-stream
  settings tags, deduping shared_ptr aliasing).
- **Hotkey is F10** — F11 is fullscreen, F9 is network player setup.
- **No Modern Vivid file ships.** A saturation-boosted palette was built,
  then reverted on review: the modern palette must stay true to the
  original. `Palette::ExpandToFullRange()` derives it from the classic
  palette; `modern.pal` remains as a per-TC override hook. With default
  worm colours the toggle is nearly invisible *by design* — modern mode's
  value is the unlocked 0..255 picker and full-precision shading.
- **Picker presents per-mode units**: classic shows the original 0..63
  numbering (`IntegerBehavior::display_div`), modern 0..255 single-step.
  Fixing this surfaced a latent `IntegerBehavior` overshoot (step past
  max) — now clamped, with defensive 0..255 clamps on TOML load and in
  the ramp math for values the bug had already persisted.
- `ColorBlock` gained a `status_index` (minimap colours at 129/133) beyond
  the planned fields.
- Three screens hardcoded `exepal` and ignored the mode (netplay connect
  screens, rollback pause/waiting overlay) — fixed to pick by mode.
- `SetColorMode` refreshes the open menu's item values (they are otherwise
  only rebuilt on menu events and went stale on toggle).
- `modernColors` is TOML-only; the binary `Settings` blob is embedded in
  replays and must keep its field layout.
- toml++ renders tables alphabetically — bit one test fixture.
- Local tooling: `scripts/clang-tidy-diff.sh` needed `--no-ext-diff` to
  survive a user-configured external diff tool.

## Overview

Unlock the worm color picker from 6 to 8 bits per channel, introduce the
`ColorMode { Classic, Modern }` toggle on `Renderer` with per-renderer
`origpal_classic` / `origpal_modern`, replace the worm-palette magic
constants with a `ColorBlock` indirection, ship one curated "Modern Vivid"
palette as data, and bump the netplay protocol version for the new color
semantics. Classic mode must stay bit-identical to today's output.

## Architecture decisions

1. **`Palette::entries` become true 8-bit.** All load paths expand to 8-bit
   at load time; `Activate` becomes a plain copy. Classic sources expand
   with `<< 2` (TGA load keeps `>> 2 << 2`, i.e. `& ~3`) so steady-state
   classic output is bit-identical to the current `Read`+`Activate`
   pipeline. `Fade`/`LightUp` move to 8-bit math (slightly *more* precision
   during transitional fades; steady state unaffected since `Fade` is a
   no-op at `amount >= 32`).
2. **Classic-mode worm ramps quantize the picked color to 6-bit and run the
   legacy `ScaleAdd` math (then `<< 2`).** This is the only way to get
   bit-identical classic worm shading; it also matches the design doc's
   per-window section ("a classic-mode renderer truncates to 6 bits per
   channel"). Modern mode runs full 8-bit math.
3. **`WormSettings.rgb` stays `int[3]`, semantics widen 0..63 → 0..255.**
   Old TOML profiles are migrated on load via an explicit format marker
   (`rgbDepth = 8` written by new builds; absent → values are 0..63 and get
   scaled ×4 with full-range expansion `v * 255 / 63`).
4. **Wire protocol: no field change, version bump only.** `PlayerInfo.rgb`
   is already `int32_t[3]`; only the value range changes, so
   `kProtocolVersion` 5 → 6 and old/new clients refuse each other with the
   existing mismatch error.
5. **`origpal` is replaced by `origpal_classic` + `origpal_modern` on
   `Renderer`, selected by `Renderer.mode` in the per-frame rebuild**
   (`gfx.cpp:940-955`). Levels that ship an embedded palette load it into
   `origpal_classic`; v1 policy: a level-custom palette is used by *both*
   modes (Modern Vivid only replaces the stock palette). Recorded as a
   deliberate v1 simplification.
6. **Modern Vivid ships as a 768-byte raw RGB palette file in `data/`**,
   same layout as classic `pal` data but loaded without the 6-bit clamp.
   v1 content: full-range expansion of the classic palette (`v * 255 / 63`,
   so whites are actually 255). Further curation is a pure data change.

## Task list

### Phase 1: Foundation (pure refactors, zero visible change)

#### Task 1: Widen `Palette` internals to 8-bit per channel

**Description:** Make `Palette::entries` hold real 8-bit values. Expand at
every load site (`Palette::Read` for level palettes, the TGA palette read in
`common.cpp`); turn `Activate` into a copy; convert `Fade`/`LightUp` to
8-bit; keep `ScaleAdd`/`SetWormColoursSpan` fed by 0..63 rgb for now by
doing legacy 6-bit math internally and shifting up (bit-identical).

**Acceptance criteria:**
- [x] Classic palette load → `Activate` produces byte-identical `real_pal`
      to the old code for all 256 entries.
- [x] Worm ramp entries are byte-identical for the default worm colors.
- [x] New Catch2 `test_palette` binary covers both (golden values computed
      from the legacy formulas inside the test).

**Verification:** build + `ctest`; `test_palette` passes; smoke-launch.

**Dependencies:** None. **Files:** `src/game/gfx/palette.{hpp,cpp}`,
`src/game/common.cpp`, `src/tests/test_palette.cpp`, `CMakeLists.txt`.
**Scope:** M.

#### Task 2: `ColorBlock` indirection for worm palette regions

**Description:** Replace `kWormSpriteColorBase[2]`, `kWormColourIndexes[2]`
and the implicit 5-wide ramp with a per-worm
`ColorBlock { base, colour_index, width }` (defaults 32/0x58 and 41/0x78,
width 5) consumed by `SetWormColour`. Pure refactor; defaults preserve
behavior exactly.

**Acceptance criteria:**
- [x] No magic palette indices left in `SetWormColour`.
- [x] `test_palette` golden worm-ramp test still passes unchanged.

**Verification:** build + `ctest`; smoke-launch.

**Dependencies:** Task 1. **Files:** `src/game/gfx/palette.{hpp,cpp}`.
**Scope:** S.

### Checkpoint A (after Tasks 1–2)
- [x] All tests pass incl. new `test_palette`; game smoke-launches; no
      visible change (worms render exactly as before).

### Phase 2: Data model + protocol

#### Task 3: Widen `WormSettings.rgb` to 0..255

**Description:** Change rgb semantics to 0..255: scale defaults
(`worm.hpp:87-89`, `settings.cpp:39`), add the `rgbDepth` TOML marker +
migration for old profiles/settings, update the picker
(`IntegerBehavior` 0..63 → 0..255, scale the color-bar width into its
168px box, faster scroll), and switch the worm-ramp input handling to
0..255 (quantizing to 6-bit internally — classic behavior, now fed
wider values).

**Acceptance criteria:**
- [x] New profiles round-trip 0..255 rgb through TOML.
- [x] An old profile (0..63, no marker) loads with the same effective color.
- [x] Picker displays and edits the full 0..255 range; bar stays in its box.
- [x] A 0..255 color that is an exact ×4 of an old 0..63 color renders the
      worm identically to the old build.

**Verification:** build + `ctest` (TOML round-trip + migration unit test);
smoke-launch and visit the player menu.

**Dependencies:** Task 1. **Files:** `src/game/worm.hpp`,
`src/game/settings.cpp`, `src/game/serialization/cereal_types.hpp`,
`src/game/gfx.cpp`, `src/game/gfx/palette.{hpp,cpp}`, a test file.
**Scope:** M.

#### Task 4: Netplay protocol bump + rgb path audit

**Description:** Bump `kProtocolVersion` 5 → 6 (`transport.hpp:27`). Audit
the rgb path (`transport.cpp:384,594`, `session.cpp:498-598`) for any 0..63
assumptions/clamps; the field is already `int32_t[3]` so no format change.

**Acceptance criteria:**
- [x] Version constant bumped with a comment noting the rgb-range change.
- [x] No 6-bit clamps remain on the net rgb path.
- [x] Version-mismatch handshake error still triggers (existing path).

**Verification:** build + `ctest`; `test_rollback_*` + `test_determinism`
still pass.

**Dependencies:** Task 3. **Files:** `src/game/net/transport.{hpp,cpp}`,
`src/game/net/session.cpp`. **Scope:** XS.

### Checkpoint B (after Tasks 3–4)
- [x] Full test suite green incl. determinism/rollback; old profile
      migration verified; picker usable in-game.

### Phase 3: Color mode + Modern Vivid + UX

#### Task 5: `ColorMode` on `Renderer`, dual `origpal`, Modern Vivid palette

**Description:** Add `ColorMode { Classic, Modern }` and `mode` to
`Renderer`; split `origpal` into `origpal_classic` / `origpal_modern`
(update all assignment sites: `renderer.cpp`, `level.cpp`,
`rematchState.cpp`, `netConnectState.cpp`, `onlineConnectState.cpp`,
`rollbackController.cpp`, `gfx.cpp`). Per-frame rebuild picks by mode.
Generate/ship the Modern Vivid palette file in `data/`, loaded unclamped at
startup (fallback: classic). Modern mode uses full 8-bit worm-ramp math;
classic keeps the quantized path.

**Acceptance criteria:**
- [x] Classic mode renders bit-identical to before (default mode).
- [ ] (manual, superseded) Forcing Modern mode + a vivid colour gives
      *subtly* different worm shading — the "visibly richer" expectation
      was dropped with the vivid-palette revert; faithful look is intended.
- [x] Missing Modern Vivid file degrades gracefully to classic.

**Verification:** build + `ctest`; smoke-launch in both modes (temporary
forced mode or the Task 6 toggle); determinism suites pass.

**Dependencies:** Tasks 1–3. **Files:** `src/game/gfx/renderer.{hpp,cpp}`,
`src/game/gfx/palette.{hpp,cpp}`, `src/game/gfx.cpp`, `src/game/level.cpp`,
state files above, `data/`. **Scope:** M.

#### Task 6: Toggle UX — settings entry, hotkey, sticky preference

**Description:** Settings hold the *default* mode (TOML-persisted, sticky);
renderers hold the *live* mode. Add a settings-menu entry and an in-game
hotkey (F11) that flips the play renderer's mode live (per-window mode
falls out of the per-renderer field).

**Acceptance criteria:**
- [x] Settings menu shows Classic/Modern; choice persists across restarts.
- [x] F11 mid-game swaps the look next frame; no glitches with fades or
      palette rotation.
- [x] Default for fresh installs: Classic.

**Verification:** build + `ctest`; smoke-launch, toggle during a match and
in menus.

**Dependencies:** Task 5. **Files:** `src/game/settings.{hpp,cpp}`,
`src/game/gfx.cpp`, `src/game/serialization/cereal_types.hpp`. **Scope:** M.

### Checkpoint C (complete)
- [x] All Stage 1 verification boxes from the design doc checked.
- [x] `test_determinism`, `test_rollback_*` green.
- [x] Smoke-launch of installed binary (per CLAUDE.md) in both modes.

## Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Classic output not bit-identical (rounding in ramp/fade math) | Med | Golden-value unit tests in `test_palette` written against legacy formulas before refactoring (Task 1) |
| Old profile migration mis-detects modern dark colors | Low | Explicit `rgbDepth` marker instead of a value heuristic |
| `origpal` split misses an assignment site | Med | grep audit for `origpal` is part of Task 5's definition of done |
| Level-embedded palettes vs Modern mode | Low | v1 policy decided up front: level palettes win in both modes |

## Open questions (deferred, not blockers)

- Modern Vivid curation beyond full-range expansion — pure data, can iterate later.
- Whether the spectator window gets its own toggle UI (the field exists; UI deferred).
