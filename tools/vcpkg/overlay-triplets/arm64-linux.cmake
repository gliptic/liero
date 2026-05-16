set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# GCC 16 made -Wincompatible-pointer-types a hard error; suppress it for
# third-party dependencies until upstream (SDL2) fixes their ALSA bindings.
set(VCPKG_C_FLAGS "-Wno-incompatible-pointer-types")
set(VCPKG_CXX_FLAGS "")
