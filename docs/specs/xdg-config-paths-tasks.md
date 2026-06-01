# Tasks: XDG-style config and data paths

Spec: [xdg-config-paths.md](xdg-config-paths.md)

Tasks are ordered by dependency. Each task should ship as its own commit
(or PR), keep `master` green, and leave the game launchable on Linux from
a build-tree run between tasks.

---

## Task 1 — `paths::` helpers  ✅ done (commit 0c7c2f2)

- [x] Add `namespace paths { FsNode userDataRoot(); FsNode systemDataRoot(); }`
      in `src/game/filesystem.hpp` / `.cpp`.
  - `userDataRoot()` wraps `SDL_GetPrefPath("openliero", "openliero")`,
    calls `create_directories` on the result, returns an `FsNode`.
  - `systemDataRoot()` resolves in order:
    1. `OPENLIERO_DATADIR` env var (runtime override for Flatpak/packagers/tests)
    2. `OPENLIERO_DATADIR` compile-time macro
    3. `SDL_GetBasePath()`
- **Acceptance**: helpers compile and link; unit test (Catch2) covers env-var
  override + fallback.
- **Verify**: `ctest --test-dir build/linux-x64` passes new tests.

---

## Task 2 — CMake compile-time `OPENLIERO_DATADIR`  ✅ done (commits 899702d, 25c7342)

- [x] In `CMakeLists.txt`, when not Emscripten, define
      `OPENLIERO_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/openliero"` on the
      `game` and `tc` libraries via `target_compile_definitions`.
  - Use `GNUInstallDirs` (`include(GNUInstallDirs)`).
  - Leave the existing flat `install(DIRECTORY data/... DESTINATION .)`
    rules unchanged in this task — install-layout reorg is Task 7.
- **Acceptance**: Building with `-DCMAKE_INSTALL_PREFIX=/tmp/foo`
  bakes `/tmp/foo/share/openliero` into the binary.
  Verified with `readelf -p .rodata build/linux-x64/Release/openliero | grep openliero-prefix`.
- **Files**: `CMakeLists.txt`.
- **Note**: The define must go on the `game` and `tc` *libraries* (not the
  executables), because `systemDataRoot()` lives in `filesystem.cpp` which
  compiles as part of those libraries. Placing it on the executables means
  `#ifdef OPENLIERO_DATADIR` never fires. Fixed in commit 25c7342.

---

## Task 3 — `paths::resolve(argc, argv)`  ✅ done (commit f4fdcbd)

- [x] Add `struct ResolvedPaths { FsNode configNode; FsNode userConfigNode;
      uint16_t port; std::vector<std::string> positionalArgs; };`
      and `ResolvedPaths paths::resolve(int argc, char* argv[],
      std::string const& basePath = {})` in `filesystem.hpp/.cpp`.
  - The `basePath` parameter overrides `SDL_GetBasePath()` and is used by tests.
- [x] Implements the algorithm from the spec:
      `--config-root` → both nodes point at that path;
      else `portable.txt` next to `SDL_GetBasePath()` → both nodes point
      at `SDL_GetBasePath()`;
      else `userConfigNode = userDataRoot()` and
      `configNode = join(userDataRoot(), systemDataRoot())`.
- [x] CLI flag parsing: `--config-root <p>`, `--port <n>`, positional args
      collected into `positionalArgs`. tctool pre-strips `--tc-name <v>`
      before calling `resolve()`.
- **Acceptance**: 9 unit tests in `src/tests/test_paths.cpp` cover env-var
  override, fallback, `--config-root`, `portable.txt`, user-shadows-system
  on read, writer routes to user, `--port` parsing, positional arg collection.
- **Files**: `src/game/filesystem.hpp`, `src/game/filesystem.cpp`,
  `src/tests/test_paths.cpp`, `CMakeLists.txt` (test target).

---

## Task 4 — Wire `paths::resolve` into gfx + gameEntry  ✅ done (commit 04aa95d)

- [x] Add `FsNode userConfigNode` next to `configNode` in `gfx.hpp`; expose
      `getUserConfigNode()`; replace `setConfigPath` with `setConfigNodes`.
- [x] In `gameEntry.cpp`, replace the manual CLI loop and
      `gfx.setConfigPath(...)` calls with a single
      `auto r = paths::resolve(argc, argv); gfx.setConfigNodes(r.configNode, r.userConfigNode);`
      block. Emscripten branch synthesises `--config-root /openliero` argv.
- [x] Settings load reads from `configNode / "Setups" / "liero.cfg"`;
      settings save writes to `userConfigNode / "Setups" / "liero.cfg"`.
- **Files**: `src/game/gfx.hpp`, `src/game/gameEntry.cpp`.

---

## Task 5 — Migrate write sites to `userConfigNode`  ✅ done (commit ddd98e5)

Audit list — switch every save/write call from `gfx->getConfigNode()` to
`gfx->getUserConfigNode()`:

- [x] `src/game/mainMenuState.cpp` (save Setups, save Profiles)
- [x] `src/game/controller/localController.cpp` (write replay)
- [x] `src/game/controller/rollbackController.cpp` (write replay)
- [x] Grep `getConfigNode()` — no remaining write sites found.
- **Files**: `src/game/mainMenuState.cpp`,
  `src/game/controller/localController.cpp`,
  `src/game/controller/rollbackController.cpp`.

---

## Task 6 — tctool + videotool share `paths::resolve`  ✅ done (commit 3fd628e)

- [x] In `src/tc_tool/tc_tool_main.cpp`, replace the inline CLI loop with
      `paths::resolve(...)` and write the converted TC under
      `r.userConfigNode / "TC" / tcName`. Pre-strip `--tc-name <v>` from
      argv before calling `resolve()` (it is not a standard flag). Also
      fixed a pre-existing bug where tctool used raw `argv[1]` instead of
      the parsed exe-path variable.
- [x] Audited `src/video_tool/tools_main.cpp`: videotool reads TC from the
      current directory and writes output next to the caller-specified
      replay path via `-r <replay>`. It makes no config-path writes.
      Documented in a comment.
- **Files**: `src/tc_tool/tc_tool_main.cpp`, `src/video_tool/tools_main.cpp`.

---

## Task 7 — CMake install layout reorg  ✅ done (commit 25c7342)

- [x] Platform-conditional install destinations:
      `WIN32 OR EMSCRIPTEN` → flat `.` layout;
      Linux/macOS → FHS `${CMAKE_INSTALL_BINDIR}` + `${CMAKE_INSTALL_DATADIR}/openliero`.
- [x] `install(TARGETS openliero/tctool/videotool DESTINATION ${OPENLIERO_INSTALL_BINDIR})`.
- [x] `install(DIRECTORY data/... DESTINATION ${OPENLIERO_INSTALL_DATADIR})`.
- [x] macOS bundle: bundle code is commented out ("app does not run for
      unknown reasons"). macOS uses the same Linux-style install (binary in
      `bin/`, stock data in `share/openliero/`). `OPENLIERO_DATADIR` baked
      at compile time handles discovery; `SDL_GetBasePath()` serves as
      fallback for zip/tarball installs where data is binary-adjacent.
      Resolves open question #3.
- [x] Updated `.github/workflows/build.yml` macOS lipo step for new `bin/`
      layout.
- **Acceptance**: `cmake --install build/linux-x64 --prefix /tmp/op` produces
  `/tmp/op/bin/openliero`, `/tmp/op/bin/tctool`, and
  `/tmp/op/share/openliero/{TC,Setups,Profiles,Resources}/`. Verified.
- **Files**: `CMakeLists.txt`, `.github/workflows/build.yml`.

---

## Task 8 — Linux + Windows packaging artifacts  ✅ done (commit 334d150)

- [x] Added `packaging/openliero.desktop` (Type=Application,
      Categories=Game;ActionGame;, Exec=openliero, Icon=openliero).
- [x] Added `packaging/openliero.metainfo.xml` (AppStream metadata;
      `appstreamcli validate` passes with one pedantic note:
      `releases-info-missing`, expected since version is generated from git).
- [x] Install both under `${CMAKE_INSTALL_DATADIR}/applications` and
      `${CMAKE_INSTALL_DATADIR}/metainfo` (Linux/macOS only, guarded
      `if(NOT WIN32)`).
- [x] Appended "Where are my saves?" section to `dist/windows/INSTALL.md`
      with MSIX-vs-zip save-path table and upgrade instructions.
- [x] Added "Upgrading from an older version" section to `README.md`
      explaining per-platform save locations and how to migrate
      Setups/Profiles/Replays/TC. Resolves open question #4.
- **Files**: `packaging/openliero.desktop`, `packaging/openliero.metainfo.xml`,
  `CMakeLists.txt`, `dist/windows/INSTALL.md`, `README.md`.

---

## Task 9 — Verification matrix  ✅ automated / 🔲 manual pending

Run the full matrix from the spec. File defects as follow-up tasks rather
than expanding this one.

- [x] Linux unit tests: all 9 `test_paths` tests + all 143 suite tests pass.
- [x] Linux `--config-root`: `paths::resolve --config-root sets both nodes`
      test passes; verified via `test_paths`.
- [x] Linux `portable.txt`: `paths::resolve portable.txt sets both nodes`
      test passes; verified via `test_paths`.
- [x] Linux installed prefix: `cmake --install` to `/tmp/openliero-prefix`
      verified correct layout; `OPENLIERO_DATADIR` baked path confirmed in
      binary via `readelf`.
- [ ] Linux build-tree: launch `openliero` from `build/linux-x64/`, confirm
      `liero.cfg` written to `$XDG_DATA_HOME/openliero` (requires display).
- [ ] macOS `.app` from `/Applications`, writes in
      `~/Library/Application Support/openliero`.
- [ ] Windows zip: extract, run, writes in `%APPDATA%\openliero\openliero\`.
- [ ] **Windows MSIX**: `Add-AppxPackage openliero.msix`, run, change setting,
      quit, relaunch, setting persists. No access-denied errors.
- [ ] Emscripten: build, serve, in-browser play works.
- [ ] tctool end-to-end: `tctool <legacy-exe>` with no flags writes TC to
      `~/.local/share/openliero/openliero/TC/<name>/`.
- **Files**: none — verification only.

---

## Done criteria for the whole feature

- [x] All 9 tasks land on master (PR #83).
- [x] Spec updated in-place when open questions resolve during implementation.
- [x] Release notes mention the new save location and the upgrade hint
      (`README.md` and `dist/windows/INSTALL.md`).
- [ ] Manual verification matrix complete on all platforms.
