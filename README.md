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
cd install/$PRESET
./openliero
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

### Extracting game data for total conversions

For copyright reasons, this repository does not contain the original Liero sound
files. Included instead is the original ruleset together with the lierolibre
sound effects.

In order to use the original data, or any total conversion, you need to run
the tctool on the game data. Example on how to do this is included below:

#### Windows

```powershell
Invoke-WebRequest https://www.liero.be/download/liero-1.36-bundle.zip -OutFile liero-1.36-bundle.zip
Expand-Archive -LiteralPath .\liero-1.36-bundle.zip .
.\tctool.exe liero-1.36-bundle
Move-Item .\TC\liero-1.36-bundle .\TC\"Liero v1.33"
Remove-Item .\liero-1.36-bundle.zip
Remove-Item -Recurse .\liero-1.36-bundle
Copy-Item -Recurse .\TC .\build\windows-x64
```

#### Linux/macOS

```bash
curl https://www.liero.be/download/liero-1.36-bundle.zip -O
unzip liero-1.36-bundle.zip
./tctool liero-1.36-bundle
mv TC/liero-1.36-bundle TC/"Liero v1.33"
rm liero-1.36-bundle.zip
rm -rf liero-1.36-bundle
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

[0]: https://github.com/gliptic/liero
[1]: https://www.libsdl.org
[2]: https://github.com/richgel999/miniz
[3]: https://www.libsdl.org/license.php
[4]: https://github.com/richgel999/miniz/blob/master/LICENSE
