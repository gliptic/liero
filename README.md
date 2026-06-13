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

## Level Authoring

Custom terrain levels can be created in classic palette-indexed format or in a
modern true-color format with optional per-pixel animation. See
[docs/modern-level-authoring.md](docs/modern-level-authoring.md) for a
step-by-step guide, including a Krita workflow and a Python script for
generating `.lev` files from painted images.

## Building

See [docs/building.md](docs/building.md) for full build instructions (native,
Emscripten/web, videotool, desync fuzzer, clang-format, clang-tidy).

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
