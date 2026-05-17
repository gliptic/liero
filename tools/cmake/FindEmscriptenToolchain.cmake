# Find the Emscripten toolchain file for use with vcpkg chainloading.
# This supports both the official EMSDK and Homebrew installations.

if(DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
  return()
endif()

# Method 1: EMSDK environment variable (official SDK)
if(DEFINED ENV{EMSDK})
  set(_emscripten_toolchain "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  if(EXISTS "${_emscripten_toolchain}")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${_emscripten_toolchain}" CACHE STRING "")
    message(STATUS "Found Emscripten toolchain via EMSDK: ${_emscripten_toolchain}")
    return()
  endif()
endif()

# Method 2: Find emcc in PATH and derive the toolchain location
find_program(_emcc_path emcc)
if(_emcc_path)
  # Resolve symlinks to get the real path
  get_filename_component(_emcc_real "${_emcc_path}" REALPATH)
  get_filename_component(_emcc_dir "${_emcc_real}" DIRECTORY)

  # Check relative to emcc's directory
  set(_emscripten_toolchain "${_emcc_dir}/cmake/Modules/Platform/Emscripten.cmake")
  if(EXISTS "${_emscripten_toolchain}")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${_emscripten_toolchain}" CACHE STRING "")
    message(STATUS "Found Emscripten toolchain via emcc: ${_emscripten_toolchain}")
    return()
  endif()

  # Homebrew: emcc is in <prefix>/bin, toolchain is in <prefix>/libexec/cmake/...
  get_filename_component(_emcc_prefix "${_emcc_dir}" DIRECTORY)
  set(_emscripten_toolchain "${_emcc_prefix}/libexec/cmake/Modules/Platform/Emscripten.cmake")
  if(EXISTS "${_emscripten_toolchain}")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${_emscripten_toolchain}" CACHE STRING "")
    message(STATUS "Found Emscripten toolchain via emcc: ${_emscripten_toolchain}")
    return()
  endif()
endif()

message(FATAL_ERROR
  "Could not find Emscripten toolchain file.\n"
  "Either:\n"
  "  - Set EMSDK environment variable and source emsdk_env.sh, or\n"
  "  - Install emscripten via your package manager (e.g. brew install emscripten), or\n"
  "  - Set -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=<path to Emscripten.cmake> manually"
)
