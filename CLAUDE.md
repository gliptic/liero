# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Open Liero — a faithful continuation of Liero 1.36 (a 2D worm shooter) ported to modern systems (SDL3) with added netplay. C++ for the game; Go for the optional signaling/relay server.

## Build / Test / Run

The build is driven by CMake presets named `<os>-<arch>` (e.g. `linux-x64`, `macos-arm64`, `windows-x64`) and an `emscripten` preset. Dependencies come from vcpkg, which is auto-bootstrapped on first configure unless `$VCPKG_ROOT` is set.

```bash
# configure + build the game
cmake --workflow --preset linux-x64                 # release
cmake --workflow --preset linux-x64-debug           # debug

# install playable bundle (binaries + game data) into install/<preset>/
cmake --install build/linux-x64 --config Release

# run
./install/linux-x64/bin/openliero

# headless smoke test (no display/audio needed); exit 124 = ran fine until timeout
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 8 ./install/linux-x64/bin/openliero
```

ctest is not a smoke test — SDL/audio/menu init paths only break when the game actually launches, so smoke-launch the installed binary before declaring UI- or startup-adjacent changes done.

Optional build targets are gated by CMake options (off by default):

- `-DOPENLIERO_BUILD_TCTOOL=ON` — extracts assets from original Liero `.exe` for total conversions
- `-DOPENLIERO_BUILD_VIDEOTOOL=ON` — renders `.lrp` replays to video (requires system ffmpeg)
- `-DOPENLIERO_BUILD_TESTS=ON` — Catch2 test suite (requires `Catch2 3` from vcpkg)

### Tests

Enable tests on the normal preset:

```bash
cmake --preset linux-x64 -DOPENLIERO_BUILD_TESTS=ON
cmake --build build/linux-x64 --config Release
ctest --test-dir build/linux-x64 --build-config Release --output-on-failure

# Single test binary (Ninja Multi-Config puts binaries in a <Config> subdir):
./build/linux-x64/Release/test_rollback_correctness

# Single Catch2 test case within a binary:
./build/linux-x64/Release/test_rollback_correctness "name of test case"
```

CI runs the same thing via the `linux-x64-ci` preset, which differs only in using `sccache` as compiler launcher and is condition-gated on `CI=true` in the environment — don't use it locally.

Each `src/tests/test_*.cpp` is wired as its own `add_executable` in `CMakeLists.txt`; if you add a test file you must add a matching block there. Several tests use `WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"` because they load assets from `data/`.

### Determinism checks

When touching anything that affects simulation determinism, run the in-tree determinism and rollback suites (each is its own Catch2 binary):

```bash
./build/linux-x64/Release/test_determinism
./build/linux-x64/Release/test_rollback_correctness
./build/linux-x64/Release/test_rollback_desync
```

These cover lockstep determinism, rollback resimulation correctness, and desync detection.

### Relay server (Go)

Lives in `server/`. Standard Go workflow:

```bash
cd server && go build -o openliero-relay && go test ./...
```

### Before committing

Run clang-format and clang-tidy on every commit that touches `src/`. Both must be clean before pushing — CI blocks on either.

```bash
# Check formatting on lines changed vs origin/master (exits 0 if clean):
scripts/clang-format-diff.sh

# Check tidy on changed lines (requires a configured build dir):
scripts/clang-tidy-diff.sh build/linux-x64

# Apply clang-format auto-fixes in-place:
git clang-format origin/master -- src/

# Apply clang-tidy auto-fixes tree-wide (use the fix script, not run-clang-tidy directly):
scripts/clang-tidy-fix.sh build/linux-x64
```

clang-format version is pinned to 22 — see the Conventions section.

## Architecture

### Top-level layout

- `src/game/` — the game library (`game` CMake target), linked by every executable
- `src/game/` `main.cpp` + `gameEntry.cpp` + `gfx.cpp` + `*State.cpp` — the playable `openliero` binary
- `src/tc_tool/` — `tctool` (asset extraction from original Liero)
- `src/video_tool/` — `videotool` (replay → video)
- `src/tests/` — Catch2 tests, one binary per file
- `server/` — Go signaling/relay server for online play
- `data/` — shipped game assets (Profiles, Resources, Setups, TC bundles); installed alongside the binary
- `tools/cmake/` — preset templates (`PresetTemplates.json`), vcpkg bootstrap, version metadata
- `packaging/`, `dist/` — platform packaging (icons, Windows AppxManifest, etc.)

`src/game/metadata.cpp` is generated at configure time by `tools/cmake/metadata.cpp.in` from git — it is `.gitignore`d, never edit it.

`CMakeLists.txt` adds `src` and `src/game` to the include path globally, so headers within `src/game/` are referenced as `"foo.hpp"` or `"subdir/foo.hpp"`.

### Game subsystems

- `Game` (`src/game/game.hpp`) is the simulation core. It owns `Level`, worms, projectiles (`nobject`, `bobject`, `sobject`), bonuses, weapons, RNG, and a `StatsRecorder`. `processFrame()` advances one tick of fully deterministic simulation.
- `Game::setSpeculative(bool)` toggles a flag that suppresses side effects (sound, stats writes) — set during rollback resim and predicted frames.
- Snapshots: `saveSnapshot/loadSnapshot` are the cereal-based correctness oracle; `saveSnapshotFast/loadSnapshotFast` write into a pre-allocated `GameSnapshot` for the rollback ring buffer (no allocation in steady state).

### Controllers (`src/game/controller/`)

Polymorphism over how the simulation is driven, all under `Controller` (`controller.hpp`):

- `LocalController` — hotseat / single-player.
- `ReplayController` — plays back `.lrp` replays.
- `RollbackController` — netplay; predicts local frames, rolls back on remote input arrival.
- `CommonController` — shared base.

`Controller::statsGame()` overrides exist because rollback runs simulation speculatively on the live game (whose `StatsRecorder` is a no-op) and keeps an authoritative *shadow* `Game` that follows confirmed frames — final stats must come from the shadow.

### Netplay (`src/game/net/` + `src/game/rollback/`)

- `NetTransport` (`transport.hpp`) — ENet-based reliable/unreliable transport.
- `NetSession` (`session.hpp`) — state machine wiring `RollbackController` to a transport: `Idle → WaitingForPeer → Handshaking → Playing → Rematch/Disconnected`.
- `ICE` + STUN + signaling — NAT traversal via libjuice; the Go server in `server/` handles signaling (and optional TURN credentials).
- `tcArchive` / `memoryFs` — TC (total conversion) data is sent over the wire and mounted in-memory on the peer.
- `RollbackController` sends batched inputs (last K = `kMaxRollback + 1` inputs per tick) tagged with a *generation* (phase counter) and the sender's `simFrame`. Receivers drop stale-generation batches and buffer next-generation ones until their own phase transition. Checksums per frame are exchanged for desync detection.

### Serialization (`src/game/serialization/`)

- Uses **cereal** for full snapshots and **toml++** for human-readable settings/profile files.
- A custom `toml_archive` adapts cereal's archive interface to toml++.
- `fast_snapshot.hpp` is the rollback-hot path; do not introduce allocation there.
- The serialization layer is adapted from gvl (BSD-2-Clause) — see README.

### Data & user paths (`src/game/filesystem.cpp`)

`paths::Resolve()` decides where the game reads and writes at startup:

- `--config-root <p>` on the command line, or a `portable.txt` next to the binary → fully portable: reads and writes go to that one directory.
- Otherwise → reads see a merged view of the per-user dir (`SDL_GetPrefPath`, e.g. `~/.local/share/openliero/openliero/` on Linux) layered over the read-only stock data next to the binary; all writes go to the per-user dir only.

`test_paths` covers this resolution; the Save As dialogs use `paths::ShadowsSystem()` to refuse names that would shadow shipped files.

### Game states

`src/game/*State.cpp` are top-level UI/flow states (`MainMenu`, `GamePlay`, `WeaponMenu`, `FileSelector`, `NetConnect`, `OnlineConnect`, `Rematch`, `Stats`). The state machine sits behind `Gfx` (`src/game/gfx.cpp`).

## Conventions

- C++ style: Google base, 100-col, `PointerAlignment: Left`, `IncludeBlocks: Preserve`. One style across all of `src/`; `.clang-format` lists only the deltas from Google defaults.
- Mechanical tree-wide reformats are listed in `.git-blame-ignore-revs`. For local `git blame` to skip them: `git config blame.ignoreRevsFile .git-blame-ignore-revs`. GitHub's blame UI honors the file automatically.
- `clang-format` runs tree-wide in CI and blocks merge on any drift (`scripts/clang-format-all.sh`). `clang-tidy` runs diff-only on PRs (against `origin/master`) and on master pushes (against the pre-push SHA) via `scripts/clang-tidy-diff.sh` for fast turnaround, plus a nightly tree-wide sweep (`scripts/clang-tidy-all.sh`, skipped if master HEAD is unchanged since the last successful nightly) and an on-demand `workflow_dispatch` for re-validating after a tidy version bump or `.clang-tidy` edit. Matching local CMake targets exist: `clang-format` / `clang-tidy` for diff-only iteration, `clang-format-all` / `clang-tidy-all` for tree-wide triage. For applying `clang-tidy` auto-fixes tree-wide use `scripts/clang-tidy-fix.sh <build-dir> [check-filter]` — it wraps `run-clang-tidy -export-fixes` with the YAML-path canonicalisation step required to avoid the per-TU header-rewrite dedup race. `.clang-tidy` enables `*` minus a list of subtractions documented at the top of the file.
- **clang-format version is pinned to 22.** CI installs `clang-format-22` from apt.llvm.org. Locally, install a v22 binary (Homebrew's `llvm` formula or apt.llvm.org) and either symlink it as `clang-format` or set `CLANG_FORMAT=clang-format-22` when running the scripts. Other versions produce subtly different output and will fail CI.
- Don't edit generated files: `src/game/metadata.cpp`, anything in `build/`, `install/`, or `tools/vcpkg/vcpkg/`.
- Determinism is load-bearing. Anything called from `Game::processFrame` (or via a controller's `process()`) must be fully deterministic across platforms — no `rand()`, no time, no floats with platform-dependent behavior, no hash-iteration order. The simulation uses the fixed-point math in `src/game/math.hpp`. If you change sim code, run the `test_rollback_*` and `test_determinism` suites.
- New tests need a matching `add_executable` + `catch_discover_tests` block in `CMakeLists.txt`; tests that read `data/` must set `WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"`.
