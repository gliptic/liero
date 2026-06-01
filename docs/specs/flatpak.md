# Spec: Flatpak packaging

## Objective

Package OpenLiero as a Flatpak for local distribution and self-hosting.
The Flatpak is an installable sandbox that gives Linux users a one-command
install without requiring system libraries.

**Not** targeting Flathub submission in this iteration — the goal is a
working, self-hostable manifest that can be built with `flatpak-builder`
and installed locally.

### Users

- Linux users who prefer Flatpak over raw tarballs.
- Distro packagers who want a reference for building a proper package.

### Success criteria

1. `flatpak-builder --force-clean build-dir packaging/io.github.openliero.openliero.yml`
   completes without error.
2. `flatpak run io.github.openliero.openliero` launches the game.
3. `flatpak run --command=tctool io.github.openliero.openliero` runs tctool.
4. Saves and settings land in `~/.var/app/io.github.openliero.openliero/data/openliero/openliero/`
   (SDL_GetPrefPath inside the Flatpak sandbox resolves there automatically).
5. Online multiplayer connects successfully (network share permitted).
6. `appstreamcli validate packaging/openliero.metainfo.xml` still passes.

## Tech Stack

- Flatpak / flatpak-builder
- Runtime: `org.freedesktop.Platform//25.08`
- SDK: `org.freedesktop.Sdk//25.08`
- Build system for game: CMake (existing), with `-DOPENLIERO_USE_VCPKG=OFF`
- SDL3 and SDL3_image come from the runtime (no bundling needed)
- Remaining C/C++ deps compiled as Flatpak modules (no vcpkg in sandbox)

## Commands

```
Build Flatpak:    flatpak-builder --force-clean build-dir \
                      packaging/io.github.openliero.openliero.yml

Install locally:  flatpak-builder --force-clean --install --user build-dir \
                      packaging/io.github.openliero.openliero.yml

Run game:         flatpak run io.github.openliero.openliero
Run tctool:       flatpak run --command=tctool io.github.openliero.openliero

Extract icon:     python3 -c "
                    from PIL import Image
                    img = Image.open('packaging/icons/openliero.icns')
                    img.save('packaging/icons/openliero-256.png')
                  "
                  # or with ImageMagick:
                  convert 'packaging/icons/openliero.icns[8]' \
                      packaging/icons/openliero-256.png

Validate metainfo: appstreamcli validate packaging/openliero.metainfo.xml
```

## Project Structure

```
packaging/
  io.github.openliero.openliero.yml  → Flatpak manifest (new)
  openliero.desktop                  → already exists
  openliero.metainfo.xml             → already exists; may need <icon> tag
  icons/
    openliero.icns                   → existing macOS icon (source for PNG)
    openliero.ico                    → existing Windows icon
    openliero-256.png                → new: extracted from .icns for Linux
```

CMakeLists.txt gains one new install rule:
```cmake
install(FILES packaging/icons/openliero-256.png
  RENAME io.github.openliero.openliero.png
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
  COMPONENT game)
```
(Only installed on non-WIN32, same guard as .desktop and .metainfo.)

## Flatpak manifest overview

App ID: `io.github.openliero.openliero`

### Sandbox permissions (`finish-args`)

```yaml
finish-args:
  - --share=ipc          # X11 shared memory (faster rendering)
  - --share=network      # online multiplayer
  - --socket=wayland     # Wayland display
  - --socket=fallback-x11
  - --socket=pulseaudio  # audio (SDL3 also supports PipeWire via PA compat)
  - --device=all         # joysticks/controllers + DRI (OpenGL/Vulkan)
```

### Module dependency order

**SDL3 (3.2.22) and SDL3_image (3.2.6) are included in freedesktop SDK 25.08**
and do not need to be compiled as modules. All remaining deps are bundled for
version-pin consistency.

Note: the runtime ships SDL3 3.2.22, whereas the vcpkg baseline pins 3.4.8.
The game must build and run correctly against the runtime's SDL3 version.
If API incompatibilities appear, either pin the vcpkg build to 3.2.22 too
or request the specific version as a bundled module (see Open Questions).

| Module | Version | Source | Notes |
|---|---|---|---|
| ~~`sdl3`~~ | ~~3.4.8~~ | ~~github.com/libsdl-org/SDL~~ | **from runtime 25.08** |
| ~~`sdl3-image`~~ | ~~3.2.6~~ | ~~github.com/libsdl-org/SDL_image~~ | **from runtime 25.08** |
| `enet` | 2.6.5 | github.com/zpl-c/enet | cmake-ninja; custom port in repo |
| `libjuice` | 1.7.1 | github.com/paullouisageneau/libjuice | cmake-ninja; `-DNO_TESTS=ON -DUSE_NETTLE=OFF` |
| `miniz` | pinned tag | github.com/richgel999/miniz | cmake-ninja |
| `tomlplusplus` | pinned tag | github.com/marzer/tomlplusplus | header-only; cmake install |
| `xxhash` | pinned tag | github.com/Cyan4973/xxHash | cmake-ninja |
| `cereal` | pinned tag | github.com/USCiLab/cereal | header-only; cmake install |
| `openliero` | local | `type: dir` pointing at repo root | last module |

### openliero module config-opts

```yaml
- -DOPENLIERO_USE_VCPKG=OFF
- -DOPENLIERO_BUILD_TCTOOL=ON
- -DOPENLIERO_BUILD_TESTS=OFF
- -DOPENLIERO_BUILD_VIDEOTOOL=OFF
- -DCMAKE_BUILD_TYPE=Release
```

`OPENLIERO_USE_VCPKG=OFF` skips the toolchain injection; `find_package` then
resolves against `/app` (the Flatpak prefix) where the modules above installed.

## Code style

The manifest uses **YAML** format (`.yml`), matching convention for
flatpak-builder. Modules are listed from deepest dependency to the app itself.
Each module pins an exact git commit SHA (not a tag) to guarantee reproducible
builds. Obtain SHAs during implementation:

```bash
git ls-remote https://github.com/zpl-c/enet refs/tags/v2.6.5
git ls-remote https://github.com/paullouisageneau/libjuice refs/tags/v1.7.1
```

## Testing Strategy

Manual verification only (no automated CI for Flatpak in this iteration):

1. Build with `flatpak-builder --force-clean --user --install build-dir manifest.yml`
2. Run game and verify it launches, audio works, joystick input works.
3. Run in offline mode to confirm stock TC loads from `/app/share/openliero/`.
4. Run online to confirm network socket is open.
5. Check save path: `ls ~/.var/app/io.github.openliero.openliero/data/`

## Boundaries

**Always**
- Pin every module to an exact commit SHA — never `branch: main`.
- Test that `appstreamcli validate` still passes after metainfo changes.
- Guard the PNG icon install under `if(NOT WIN32)` same as .desktop.

**Ask first**
- Enabling any additional Flatpak permissions beyond those listed above.
- Splitting tctool into its own Flatpak extension.
- Adding a CI job for Flatpak builds (slow; needs self-hosted runner or
  large GitHub runner).

**Never**
- Pass `--allow-emulated-io` or `--filesystem=host` — keep the sandbox.
- Use `type: archive` with an unversioned URL — always pin the SHA.

## Resolved Questions

1. ~~**SDL3_image version**~~ ✅ SDL3_image 3.2.6 is in the 25.08 runtime.
   No bundling needed.
2. ~~**libjuice patch**~~ ✅ The `dependencies.diff` patch is needed even
   with `-DUSE_NETTLE=OFF`. It adds `find_dependency(Threads)` to
   `LibJuiceConfig.cmake.in`; without it downstream consumers (openliero)
   fail to link. Patch kept in `packaging/patches/libjuice-dependencies.diff`.
3. ~~**Commit SHAs**~~ ✅ All SHAs resolved and documented in the manifest.
   enet uses `zpl-c/enet` (overlay port) not `lsalzman/enet` (main registry).
4. ~~**Icon quality**~~ ✅ Used 512×512 (ic09 chunk from the icns).
   Flatpak icon validation caps at 512×512 — 1024×1024 is rejected.
5. ~~**SDL3 version mismatch**~~ ✅ SDL3 3.2.22 from the runtime builds and
   runs the game without API issues. No bundled SDL3 needed.
