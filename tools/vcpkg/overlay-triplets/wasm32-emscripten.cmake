set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)

set(VCPKG_ENV_PASSTHROUGH EMSDK PATH)

# Find the Emscripten toolchain for vcpkg to use
if(NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
  # Method 1: EMSDK environment variable
  if(DEFINED ENV{EMSDK})
    set(_toolchain "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
    if(EXISTS "${_toolchain}")
      set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${_toolchain}")
    endif()
  endif()

  # Method 2: Find emcc in PATH
  if(NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
    find_program(_emcc emcc)
    if(_emcc)
      get_filename_component(_emcc_real "${_emcc}" REALPATH)
      get_filename_component(_emcc_dir "${_emcc_real}" DIRECTORY)

      # emcc next to cmake/ (official EMSDK or Homebrew libexec)
      if(EXISTS "${_emcc_dir}/cmake/Modules/Platform/Emscripten.cmake")
        set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${_emcc_dir}/cmake/Modules/Platform/Emscripten.cmake")
      else()
        # Homebrew: emcc in <prefix>/bin, toolchain in <prefix>/libexec/cmake/...
        get_filename_component(_prefix "${_emcc_dir}" DIRECTORY)
        if(EXISTS "${_prefix}/libexec/cmake/Modules/Platform/Emscripten.cmake")
          set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${_prefix}/libexec/cmake/Modules/Platform/Emscripten.cmake")
        endif()
      endif()
    endif()
  endif()
endif()
