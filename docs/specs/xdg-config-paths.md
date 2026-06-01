# Spec: XDG-style config and data paths

## Objective

Make Open Liero suitable for standard OS packaging (Linux distros, macOS .app
bundles, Windows installers) by separating **read-only stock data** (shipped
with the binary) from **writable user data** (saves, replays, custom TCs).
Today everything lives in a single directory next to the executable, which
breaks any install where that directory is not writable (system-wide installs
to `/usr/bin`, `/opt`, `Program Files`, signed macOS bundles, Windows MSIX,
etc.) and litters the user's CWD when invoked from anywhere else.

### Users

- **End users** installing via package managers (`dnf`, `apt`, `brew`, Flatpak,
  winget, etc.) expect the game to write to their home directory, not into the
  install prefix.
- **End users** extracting a zip / tarball expect the same behaviour without
  any setup: stock TCs come from the bundle, their own saves go to a sane
  user location, the bundle stays untouched.
- **Power users** who want today's all-in-one-folder behaviour (USB stick,
  Emscripten) can opt in with a marker file or `--config-root`.
- **Modders** using `tctool` to convert a legacy `LIERO.EXE` expect the
  resulting TC to land where the game will find it on next launch.

### Success criteria

1. A `cmake --install` to `/usr/local` (or a Linux distro packaging build)
   places `openliero` in `<prefix>/bin/`, stock data in
   `<prefix>/share/openliero/`, and the running game writes nothing under
   `<prefix>`.
2. Launching `openliero` from any working directory with no flags finds stock
   TCs and writes `liero.cfg` / replays / custom TCs / custom setups /
   custom profiles to `SDL_GetPrefPath("openliero", "openliero")` (which
   resolves to `$XDG_DATA_HOME/openliero` on Linux,
   `~/Library/Application Support/openliero` on macOS,
   `%APPDATA%\openliero\openliero\` on Windows desktop, and the MSIX
   per-package virtual folder under the signed MSIX install).
3. In-game file selectors (TC, Setups, Profiles, Replays) show the **union**
   of stock and user content; saving from the menu always writes to the user
   path; a user-side name shadows a same-named stock entry on read.
4. `tctool` uses the **same** path-resolution logic as the main binary:
   `--config-root` overrides, otherwise `portable.txt` next to the executable
   selects single-dir mode, otherwise it writes its converted TC into the
   user data path under `TC/<name>`. The next `openliero` launch (with the
   same resolution) lists it without further setup.
5. Dropping a `portable.txt` next to the executable restores today's
   single-directory behaviour (read and write both from the binary dir).
   Used by the zip-only crowd that prefers a self-contained folder, and by
   the Emscripten build implicitly.
6. `--config-root <path>` continues to force a single directory for both
   read and write (no XDG, no system path). Used by Emscripten and CI.
7. Behaviour is verified on Linux, macOS, Windows (zip + MSIX), and
   Emscripten before merge.

## Tech Stack

- C++ (existing), SDL3, CMake, vcpkg.
- Runtime dependency: `SDL_GetPrefPath` and `SDL_GetBasePath` (SDL3).

## Commands

```
Configure & build: cmake --workflow --preset linux-x64
Install:           cmake --install build/linux-x64 --config Release
Tests:             ctest --test-dir build/linux-x64 --output-on-failure
Run game:          ./install/linux-x64/openliero
Run tctool:        ./install/linux-x64/tctool <path-to-legacy-tc>
```

## Project Structure

No new top-level directories. Touched files:

```
src/game/filesystem.hpp        → declare path-resolution helpers
src/game/filesystem.cpp        → implement path-resolution helpers (SDL3-based)
src/game/gfx.hpp               → swap configNode for joined (user + system) node;
                                 add a separate userConfigNode for write sites;
                                 replace setConfigPath with setConfigNodes
src/game/gameEntry.cpp         → call paths::resolve(); wire configNode +
                                 userConfigNode into gfx; Emscripten synthesises
                                 --config-root /openliero argv
src/game/fileSelectorState.cpp → no semantic change (rely on FsNodeJoin)
src/game/controller/localController.cpp,
src/game/controller/rollbackController.cpp,
src/game/mainMenuState.cpp     → writes go through userConfigNode, not the
                                 joined node, so writes never land in the system dir
src/tc_tool/tc_tool_main.cpp   → share path-resolution with main binary
                                 (--config-root, portable.txt, else user dir)
src/video_tool/tools_main.cpp  → audit + same treatment if it writes
CMakeLists.txt                 → split install into CMAKE_INSTALL_BINDIR /
                                 CMAKE_INSTALL_DATADIR; pass install datadir to
                                 the build as a compile-time define
packaging/openliero.desktop    → new: freedesktop .desktop entry
packaging/openliero.metainfo.xml → new: AppStream metadata
dist/windows/INSTALL.md        → add a short note documenting save location
                                 (MSIX per-package store vs portable zip)
dist/windows/AppxManifest.xml.in → no change expected; verify the new path
                                 logic works under MSIX's read-only install dir
docs/specs/xdg-config-paths.md → this spec
docs/specs/xdg-config-paths-tasks.md → task list
```

## Code Style

Path resolution lives behind small helpers in `filesystem.hpp` so call sites
stay readable:

```cpp
namespace paths {
    // Writable user data root. Always exists (created on demand).
    // Equivalent to SDL_GetPrefPath("openliero", "openliero").
    FsNode userDataRoot();

    // Read-only stock data root. May be empty if no stock data shipped.
    // Resolution order:
    //   1. OPENLIERO_DATADIR env var (Flatpak / packagers / tests)
    //   2. OPENLIERO_DATADIR compile-time macro (e.g. /usr/share/openliero)
    //   3. SDL_GetBasePath()  -- binary-adjacent, for zip/tarball/.app
    FsNode systemDataRoot();
}
```

Naming: lowerCamelCase for functions, matching existing style.

## Path resolution algorithm

Final `gfx.configNode` is built like this:

```
if --config-root <p> given:
    configNode     = FsNode(p)
    userConfigNode = FsNode(p)             // all writes go here
elif portable.txt next to executable:
    configNode     = FsNode(SDL_GetBasePath())
    userConfigNode = FsNode(SDL_GetBasePath())
else:
    user   = paths::userDataRoot()
    system = paths::systemDataRoot()       // may be null
    configNode     = join(user, system)    // FsNodeJoin: user wins on reads
    userConfigNode = user                  // writes never touch system dir
```

`FsNodeJoin` already exists (`src/game/filesystem.cpp:408`) with the exact
semantics needed: `iter()` unions both listings, `go()` recursively joins,
`tryToReader()` prefers `a` (user), `tryToWriter()` prefers `a` (user) and
falls through to `b` only if `a` is not writable.

This whole resolution block lives in `filesystem.cpp` (e.g.
`paths::resolve(argc, argv)` returning `{configNode, userConfigNode, ...}`)
so both `openliero` and `tctool` share one implementation. tctool only
needs `userConfigNode` (it never reads stock data) but goes through the
same entry point so flag handling and `portable.txt` behaviour stay
identical.

**Read sites** (`configNode`) stay unchanged structurally — the union is
transparent. **Write sites** (`liero.cfg`, custom Setups, Profiles, Replays,
tctool output) must be migrated to use `userConfigNode` so a writable system
dir (e.g. running from CWD with `portable.txt` absent but `./TC` happening
to exist due to a dev checkout) can't accidentally receive writes meant for
the user dir.

## Testing Strategy

Existing test framework lives in `src/tests/` (Catch2). Add unit tests
covering:

- `paths::systemDataRoot()` honours the env-var override and falls back
  when the override path doesn't exist (Task 1, done).
- `paths::resolve()` integration tests with two temp dirs covering user
  shadowing system on read, writes routing to user, `portable.txt`
  short-circuit, explicit `--config-root` override (Task 3).
- `FsNodeJoin` correctness (already exists; add coverage if missing).

Manual verification matrix (Success Criteria #7, Task 9):

| Platform   | Scenario                                              | Expected                                              |
|------------|-------------------------------------------------------|-------------------------------------------------------|
| Linux      | `cmake --install` to `/tmp/prefix`, run from anywhere | Reads stock TC from `/tmp/prefix/share/openliero`, writes to `$XDG_DATA_HOME/openliero` |
| Linux      | Run from build dir / extracted zip                    | Reads from binary-adjacent `data/`, writes to user dir |
| Linux      | `portable.txt` present                                | All reads + writes in binary dir                       |
| Linux      | `--config-root /tmp/foo`                              | All reads + writes in `/tmp/foo`                       |
| macOS      | `.app` bundle launched from `/Applications`           | Reads from bundle Resources, writes to `~/Library/Application Support/openliero` |
| Windows    | Extracted zip from any folder                         | Reads from binary-adjacent data, writes to `%APPDATA%\openliero\openliero` |
| Windows    | MSIX install (`Add-AppxPackage openliero.msix`)       | Reads from read-only package dir, writes to MSIX per-package virtual folder; no access-denied errors |
| Emscripten | Browser                                                | All reads + writes in `/openliero` (existing behaviour) |
| tctool     | `tctool ~/old-liero` with no flags                    | Writes to `~/.local/share/openliero/TC/old-liero/`    |

## Boundaries

**Always**
- Use `paths::userDataRoot()` / `paths::systemDataRoot()` and never call
  `SDL_GetPrefPath` / `SDL_GetBasePath` directly from outside `filesystem.cpp`.
- Route every write through `userConfigNode`, never through the joined `configNode`.
- Create the user data directory tree lazily on first write.

**Ask first**
- Changing the SDL_GetPrefPath organization/app strings later (would orphan
  existing users' data).
- Renaming `--config-root` or removing it.
- Adding a third path tier (e.g. an `XDG_CONFIG_HOME` split separating
  `liero.cfg` from bulk data). Considered and rejected for v1.
- Migration tooling to copy legacy in-place data into user dir on first
  run. Explicitly excluded from this spec.

**Never**
- Write into `systemDataRoot()` from the running game or from tctool.
- Hard-code `/usr/share/openliero` — always go through `OPENLIERO_DATADIR`
  as set by the CMake install layout.
- Silently swallow a missing system path; if neither system nor user has
  the requested TC, fail with a clear error as today.

## Resolved Questions

1. **`videotool` output path.** ✅ Resolved. videotool takes a
   caller-specified `-r <replay>` path and writes output files
   (`<replay>_n.mp4` / `_s.mp4`) next to the replay. It reads the TC from
   the current directory. It makes no writes to any config path. Documented
   in a code comment in `src/video_tool/tools_main.cpp`.

2. **Flatpak / AppImage.** ✅ No extra code needed. `SDL_GetPrefPath`
   returns the sandboxed `~/.var/app/<id>/data/openliero/openliero/` path
   automatically inside a Flatpak sandbox. The AppStream metainfo file
   (`packaging/openliero.metainfo.xml`) is now included and installed, which
   is a prerequisite for Flathub submission.

3. **macOS bundle layout.** ✅ Resolved. The macOS `.app` bundle code is
   commented out ("app does not run for unknown reasons") — macOS currently
   uses the same Linux-style FHS install: binary in `bin/`, stock data in
   `share/openliero/`. The `OPENLIERO_DATADIR` compile-time macro baked by
   CMake covers installed builds. `SDL_GetBasePath()` (which returns the
   directory containing the binary) serves as fallback for zip/tarball
   installs where data is adjacent to the binary. No extra adjustment needed
   for the non-bundle case. If the bundle is ever revived, `SDL_GetBasePath()`
   inside a `.app` returns `Contents/MacOS/`, so stock data would need to be
   placed there or the lookup extended to check `../Resources/`.

4. **Legacy data migration.** ✅ Resolved. "Upgrading from an older version"
   section added to `README.md` and `dist/windows/INSTALL.md`, explaining
   per-platform save locations and how to copy `Setups/`, `Profiles/`,
   `Replays/`, and custom `TC/` from an old single-directory install.
