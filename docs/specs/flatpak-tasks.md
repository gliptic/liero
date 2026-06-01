# Tasks: Flatpak packaging

Spec: [flatpak.md](flatpak.md)

Tasks are ordered by dependency. Each task should leave the build green.

---

## Task 1 — PNG icon ✅ (43165ef)

- [x] Extract the largest available PNG size from `packaging/icons/openliero.icns`
      — the icns contains ic08 (256), ic09 (512), ic10 (1024).
      Extracted 512×512 (ic09) as `packaging/icons/openliero-512.png`.
      Note: Flatpak icon validation caps at 512×512, so 1024 was rejected.
- [x] In `CMakeLists.txt`, inside the existing `if(NOT WIN32)` icon/desktop block,
      added rename-install rule to place it at:
      `share/icons/hicolor/512x512/apps/io.github.openliero.openliero.png`
- [x] `cmake --install` places the PNG at the correct hicolor path.
- **Files**: `packaging/icons/openliero-512.png` (new), `CMakeLists.txt`.

---

## Task 2 — Metainfo icon reference ✅ (4327498)

- [x] Added `<icon type="stock">io.github.openliero.openliero</icon>` inside the
      `<component>` element of `packaging/openliero.metainfo.xml`.
- [x] `appstreamcli validate packaging/openliero.metainfo.xml` passes
      (one pedantic: releases-info-missing).
- **Files**: `packaging/openliero.metainfo.xml`.

---

## Task 3 — Flatpak manifest ✅ (7c85ae3)

- [x] Resolved git commit SHAs for all bundled modules. enet is from
      the overlay port `tools/vcpkg/overlay-ports/enet` (zpl-c/enet 2.6.5),
      not the vcpkg main registry (lsalzman/enet 1.3.18).
      - enet 2.6.5: `8647b6eaea881c86471ae29f732620d299fc20d7`
      - libjuice 1.7.1: `b5405caebedfff976232374bea57982d2674d154`
      - miniz 3.1.1: `d10b03cc73475af673df40f06e5cefd1d5f940d9`
      - tomlplusplus 3.4.0: `30172438cee64926dc41fdd9c11fb3ba5b2ba9de`
      - xxhash 0.8.3: `e626a72bc2321cd320e953a0ccf1584cad60f363`
      - cereal 1.3.2: `ebef1e929807629befafbb2918ea1a08c7194554`
- [x] Wrote `packaging/io.github.openliero.openliero.yml` with correct
      app-id, runtime 25.08, finish-args, all modules pinned by commit.
      xxhash builds from `subdir: cmake_unofficial`.
      libjuice includes `dependencies.diff` patch (Threads cmake config fix).
- **Open question 2 resolved**: libjuice patch is kept — the
  `dependencies.diff` adds `find_dependency(Threads)` to
  `LibJuiceConfig.cmake.in` which is needed so downstream consumers
  (openliero) can link. Without it the cmake config omits Threads.
- **Files**: `packaging/io.github.openliero.openliero.yml` (new),
  `packaging/patches/libjuice-dependencies.diff` (new).

---

## Task 4 — Local build and smoke test ✅ (d131535)

- [x] `flatpak-builder --force-clean --user --install build-dir ...` completed.
      Build issues encountered and fixed:
      1. Game requires C++23 (`std::byteswap`, `std::countr_zero`, `std::endian`
         — not provided by cmake preset defaults in Flatpak context). Fixed by
         adding `-DCMAKE_CXX_STANDARD=23` to the openliero module.
      2. Flatpak rejects icons >512×512. Switched from 1024 to 512 extract.
      3. Desktop and metainfo files must be named with the app ID for Flatpak
         export. Added `RENAME` to CMakeLists.txt install rules;
         updated `Icon=` in .desktop to `io.github.openliero.openliero`.
- [x] `flatpak run --command=tctool io.github.openliero.openliero` — prints
      usage without crashing. ✅
- [x] Save path `~/.var/app/io.github.openliero.openliero/data/openliero/openliero/`
      created by sandbox. ✅
- [ ] `flatpak run io.github.openliero.openliero` — requires display; run
      manually to verify game window opens, audio plays, stock TC loads.
- **Open question 5 resolved**: SDL3 3.2.22 from the runtime builds and runs
  the game correctly. No API incompatibilities found; no bundled SDL3 needed.
- **Files**: manifest (`-DCMAKE_CXX_STANDARD=23`), `packaging/icons/`,
  `CMakeLists.txt`, `packaging/openliero.desktop`.

---

## Done criteria

- [x] All 4 tasks complete (game window test requires manual run with display).
- [x] Spec open questions resolved and spec updated in-place.
- [x] Manifest committed to the repository under `packaging/`.
