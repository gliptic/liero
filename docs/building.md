# Building OpenLiero

## Prerequisites

* git
* cmake
* ninja

## Native build

```bash
PRESET=$OS-$ARCH
# OS:   macos, linux, windows
# ARCH: x64, arm64

# configure & build
cmake --workflow --preset $PRESET

# install binaries + game data to install/$PRESET/
cmake --install build/$PRESET --config Release

# play
./install/$PRESET/bin/openliero        # Linux / macOS
./install/$PRESET/openliero.exe        # Windows (flat layout)
```

Debug build:

```bash
cmake --workflow --preset $PRESET-debug
cmake --install build/$PRESET --config Debug
```

This builds `openliero` and `tctool`. For `videotool` see below.

## Web build (Emscripten)

### Prerequisites

* [Emscripten](https://emscripten.org/) (via [EMSDK](https://emscripten.org/docs/getting_started/downloads.html)
  or your package manager, e.g. `brew install emscripten`)

### Build

```bash
cmake --preset emscripten
cmake --build build/emscripten --config Release
cmake --install build/emscripten --config Release
```

Output is in `install/emscripten/`. Serve it with any HTTP server:

```bash
python3 -m http.server -d install/emscripten
```

Then open `openliero.html` in a browser.

## videotool

`videotool` converts `.lrp` replay files to video. It requires ffmpeg
development libraries on the system.

```bash
cmake --preset $PRESET -DOPENLIERO_BUILD_VIDEOTOOL=ON
cmake --build build/$PRESET --target videotool
```

Usage:

```bash
# Single replay
./videotool -r path/to/replay.lrp

# With spectator view
./videotool -s -r path/to/replay.lrp

# All replays in a directory
./videotool -d -r "path/to/replays/*.lrp"
```

## Desync fuzzer

Stress-tests multiplayer determinism by running randomized game simulations in
parallel and checking for state divergence.

```bash
cmake --preset $PRESET -DOPENLIERO_BUILD_FUZZER=ON
cmake --build build/$PRESET --target desync_fuzzer
```

Usage:

```bash
# Default (100 iterations, 30k frames, 16 workers)
./build/$PRESET/desync_fuzzer

# Quick smoke test
./build/$PRESET/desync_fuzzer --iterations 10 --frames 5000

# Long run with more parallelism
./build/$PRESET/desync_fuzzer --iterations 1000 --frames 30000 -j 32

# Simulated network jitter
./build/$PRESET/desync_fuzzer --iterations 100 --frames 10000 --jitter 5

# Reproduce a specific failure
./build/$PRESET/desync_fuzzer --seed 12345 --iterations 1
```

Options:
- `--iterations N` — number of randomized iterations (default: 100)
- `--frames N` — simulation frames per iteration (default: 30000)
- `--seed N` — fixed base seed for reproducibility (default: time-based)
- `--jobs N`, `-j N` — parallel worker threads (default: 16)
- `--jitter N` — random packet delivery delay of 0–N ticks (default: 0)

## Linting and formatting

CI runs `clang-format` tree-wide on every PR and blocks merge on any drift,
plus `clang-tidy` diff-only on PRs and master pushes (with a nightly tree-wide
sweep).

### clang-format

The clang-format version is **pinned to 22**. Install a v22 binary (Homebrew's
`llvm` formula or [apt.llvm.org](https://apt.llvm.org/)) and either symlink it
as `clang-format` or set `CLANG_FORMAT=clang-format-22`.

```bash
# diff-only — changes on your branch vs origin/master
scripts/clang-format-diff.sh

# tree-wide — every file under src/ (matches CI)
scripts/clang-format-all.sh
```

CMake targets (configure any preset first):

```bash
cmake --build build/$PRESET --target clang-format       # diff-only
cmake --build build/$PRESET --target clang-format-all   # tree-wide
```

### clang-tidy

Requires a build directory with `compile_commands.json`. Any preset works
locally; CI uses `linux-x64-ci`.

```bash
# diff-only — fast, mirrors the PR check
scripts/clang-tidy-diff.sh build/$PRESET

# tree-wide — slow, mirrors the nightly sweep
scripts/clang-tidy-all.sh build/$PRESET
```

CMake targets:

```bash
cmake --build build/$PRESET --target clang-tidy       # diff-only
cmake --build build/$PRESET --target clang-tidy-all   # tree-wide
```

To apply auto-fixes tree-wide (modifies source files — run on a clean working
tree):

```bash
# All checks .clang-tidy enables
scripts/clang-tidy-fix.sh build/$PRESET

# Scope to one check or family
scripts/clang-tidy-fix.sh build/$PRESET '-*,modernize-use-override'
```

This wraps `run-clang-tidy -export-fixes` with a path-canonicalisation pass
before `clang-apply-replacements` to avoid a per-TU header-rewrite race in
clang-tidy 22.

### git blame past reformats

To skip mechanical reformats in `git blame`:

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

GitHub's blame UI honours the file automatically.
