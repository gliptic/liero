# Open Liero

Earthworm simulation game based on a real physical model. Features: 2 worms,
40 weapons, great playability. Four game modes: Kill'em All, Game of Tag,
Capture the Flag and Simple CtF!

This is a continuation of the openliero project, originally located at
[github.com/gliptic/liero][0], also known as Liero 1.36.

The main ambition is to keep Liero running as faithfully as possible on
modern machines, although we are not opposed to making careful changes
and improvements to the original game (especially if they are optional).

Patches are welcome!

## Downloads

Download the latest release from the [releases page][releases].

### Portable archives

The portable archives are the easiest way to run OpenLiero. Extract the archive
anywhere and run `openliero`. All saves, replays, profiles, and settings stay
inside the extracted folder — nothing is written elsewhere.

| Archive | Platform |
|---|---|
| `openliero-linux-x64-portable.tar.gz` | Linux x86_64 |
| `openliero-macos-portable.tar.gz` | macOS (arm64 + x86_64 universal) |
| `openliero-windows-x64-portable.zip` | Windows x86_64 |
| `openliero-web.tar.gz` | Web (self-hosted Emscripten/WebAssembly) |

Each archive ships with a `portable.txt` file next to the binary. When that
file is present, OpenLiero reads and writes everything from the same folder. To
switch to per-user save locations (so saves survive reinstalls or live in your
home directory), simply delete `portable.txt`.

### Windows MSIX installer

The MSIX package provides a Start Menu entry and clean uninstall via
Settings → Apps. Because Windows MSIX install directories are read-only,
OpenLiero saves to `%APPDATA%\openliero\openliero\` instead of the install
directory.

#### Step 1 — Verify and trust the publisher certificate

Download `openliero-publisher.cer` from the release page and verify its
thumbprint before importing it:

```powershell
$cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 "openliero-publisher.cer"
$cert.Thumbprint
# Expected: 3E2C2090E1D6A075DD8A480B1FA858E503730498
```

If the thumbprint matches, import it to the Trusted People store:

```powershell
certutil -addstore "TrustedPeople" openliero-publisher.cer
```

#### Step 2 — Install the MSIX

Double-click `openliero.msix`, or from PowerShell:

```powershell
Add-AppxPackage openliero.msix
```

#### Uninstalling

```powershell
Get-AppxPackage *openliero* | Remove-AppxPackage
```

Or use Settings → Apps → Installed apps → OpenLiero → Uninstall.

#### Why self-signed?

OpenLiero is a free, open-source project and does not have a paid code-signing
certificate. We publish the certificate thumbprint above so you can verify it
before importing. Build provenance can be verified independently with the
[GitHub CLI][5]:

```
gh attestation verify <artifact-file> --repo openliero/openliero
```

## Extracting game data for total conversions

For copyright reasons, this repository does not contain the original Liero sound
files. Included instead is the original ruleset together with the lierolibre
sound effects.

To use the original data, or any total conversion, run `tctool` on the game
data. The examples below assume you are running from inside the extracted
portable archive, where `portable.txt` causes `tctool` to write the TC into
the same folder.

### Windows

```powershell
Invoke-WebRequest https://www.liero.be/download/liero-1.36-bundle.zip -OutFile liero-1.36-bundle.zip
Expand-Archive -LiteralPath .\liero-1.36-bundle.zip .
.\tctool.exe liero-1.36-bundle
Rename-Item .\TC\liero-1.36-bundle "Liero v1.33"
Remove-Item .\liero-1.36-bundle.zip
Remove-Item -Recurse .\liero-1.36-bundle
```

### Linux/macOS

```bash
curl https://www.liero.be/download/liero-1.36-bundle.zip -O
unzip liero-1.36-bundle.zip
./tctool liero-1.36-bundle
mv TC/liero-1.36-bundle TC/"Liero v1.33"
rm -rf liero-1.36-bundle.zip liero-1.36-bundle
```

## Building

### Prerequisites

* git
* cmake
* ninja

```bash
PRESET=$OS-$ARCH
# where OS is one of the following: macos, linux, windows
# and ARCH is one of the following: x64, arm64

# configure & build all in one command
cmake --workflow --preset $PRESET

# copy binaries and game data to a predefined directory (install/$PRESET)
cmake --install build/$PRESET --config Release

# play
./install/$PRESET/bin/openliero        # Linux / macOS
./install/$PRESET/openliero.exe        # Windows (flat layout)
```

For a debug build:

```bash
cmake --workflow --preset $PRESET-debug
cmake --install build/$PRESET --config Debug
```

Note: This only builds `openliero` & `tctool`, not `videotool`. See below for
videotool build instructions.

### Building for the web (Emscripten)

You can build Open Liero as a WebAssembly application that runs in the browser.

#### Prerequisites

* [Emscripten](https://emscripten.org/) (via [EMSDK](https://emscripten.org/docs/getting_started/downloads.html) or your package manager, e.g. `brew install emscripten`)

#### Build

```bash
cmake --preset emscripten
cmake --build build/emscripten --config Release
cmake --install build/emscripten --config Release
```

The output is in `install/emscripten/` — serve it with any HTTP server
(e.g. `python3 -m http.server -d install/emscripten`) and open
`openliero.html` in a browser.

### Building videotool

The videotool converts `.lrp` replay files to video. It requires ffmpeg
development libraries installed on your system.

#### Build

```bash
cmake --preset $PRESET -DOPENLIERO_BUILD_VIDEOTOOL=ON
cmake --build build/$PRESET --target videotool
```

#### Usage

```bash
# Convert a single replay
./videotool -r path/to/replay.lrp

# Convert with spectator view
./videotool -s -r path/to/replay.lrp

# Convert all replays in a directory
./videotool -d -r "path/to/replays/*.lrp"
```

### Building the desync fuzzer

The desync fuzzer stress-tests multiplayer determinism by running randomized
game simulations in parallel and checking for state divergence.

#### Build

```bash
cmake --preset $PRESET -DOPENLIERO_BUILD_FUZZER=ON
cmake --build build/$PRESET --target desync_fuzzer
```

#### Usage

```bash
# Run with defaults (100 iterations, 30k frames each, 16 parallel workers)
./build/$PRESET/desync_fuzzer

# Quick smoke test
./build/$PRESET/desync_fuzzer --iterations 10 --frames 5000

# Long run with more parallelism
./build/$PRESET/desync_fuzzer --iterations 1000 --frames 30000 -j 32

# Test with simulated network jitter (random packet delay)
./build/$PRESET/desync_fuzzer --iterations 100 --frames 10000 --jitter 5

# Reproduce a specific failure
./build/$PRESET/desync_fuzzer --seed 12345 --iterations 1
```

Options:
- `--iterations N` — number of randomized iterations (default: 100)
- `--frames N` — simulation frames per iteration (default: 30000)
- `--seed N` — fixed base seed for reproducibility (default: time-based)
- `--jobs N`, `-j N` — parallel worker threads (default: 16)
- `--jitter N` — random packet delivery delay of 0-N ticks (default: 0 = instant)

### Linting and formatting

CI runs `clang-format` tree-wide on every PR and blocks merge on any drift,
plus `clang-tidy` diff-only on PRs and master pushes (with a nightly tree-wide
sweep). You can run the same checks locally before pushing.

#### clang-format

The clang-format version is **pinned to 22**. Other versions produce subtly
different output and will fail CI. Install a v22 binary (Homebrew's `llvm`
formula or [apt.llvm.org](https://apt.llvm.org/)) and either symlink it as
`clang-format` or export `CLANG_FORMAT=clang-format-22`.

```bash
# diff-only — what would change on your branch vs origin/master
scripts/clang-format-diff.sh

# tree-wide — every file under src/ (matches CI)
scripts/clang-format-all.sh
```

Matching CMake targets (configure any preset first):

```bash
cmake --build build/$PRESET --target clang-format       # diff-only
cmake --build build/$PRESET --target clang-format-all   # tree-wide
```

#### clang-tidy

Requires a build directory with `compile_commands.json`. The `linux-x64-ci`
preset is what CI uses; locally any preset works.

```bash
cmake --preset linux-x64-ci -DOPENLIERO_BUILD_TESTS=ON \
  -DOPENLIERO_BUILD_VIDEOTOOL=ON -DOPENLIERO_BUILD_TCTOOL=ON
cmake --build build/linux-x64-ci --target game

# diff-only — fast, mirrors the PR check
scripts/clang-tidy-diff.sh build/linux-x64-ci

# tree-wide — slow, mirrors the nightly sweep
scripts/clang-tidy-all.sh build/linux-x64-ci
```

Matching CMake targets:

```bash
cmake --build build/linux-x64-ci --target clang-tidy       # diff-only
cmake --build build/linux-x64-ci --target clang-tidy-all   # tree-wide
```

To git-blame past mechanical reformats, point blame at the ignore list once:

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

## License

Open Liero is licensed with the [BSD-2-Clause](LICENSE). Since the copyright
for the default sounds in Liero 1.33 is unknown, this project uses the sound
pack from LibreLiero, which is licensed with the
[WTFPL](data/TC/openliero/sounds/LICENSE).

This project depends on various other open source libraries which are licensed
under their own respective licenses:

* [SDL + SDL_image][1] ([zlib][3])
* [miniz][2] ([MIT][4])

The serialization layer in `src/game/serialization/` is adapted from
[gvl](https://github.com/gliptic/gvl) by Erik Lindroos and Martin Erik Werner
(BSD-2-Clause).

[0]: https://github.com/gliptic/liero
[1]: https://www.libsdl.org
[2]: https://github.com/richgel999/miniz
[3]: https://www.libsdl.org/license.php
[4]: https://github.com/richgel999/miniz/blob/master/LICENSE
[5]: https://cli.github.com/
[releases]: https://github.com/openliero/openliero/releases
