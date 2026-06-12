cmake_minimum_required(VERSION 3.25..3.30)

# options at top so they're easy to find
option(OPENLIERO_BUILD_TCTOOL "Build tctool" OFF)
option(OPENLIERO_BUILD_VIDEOTOOL "Build videotool" OFF)
option(OPENLIERO_USE_VCPKG "Use vcpkg to manage dependencies" ON)
option(OPENLIERO_ENABLE_CLANG_TIDY "Run clang-tidy during every C++ compile" OFF)

# tell cmake we have goodies in the cmake/ directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")

include(GenerateVersion) # generate version metadata from git
include(GNUInstallDirs)

# configure vcpkg
if(OPENLIERO_USE_VCPKG)
  # For Emscripten builds, find and set the chainload toolchain before vcpkg
  if(VCPKG_TARGET_TRIPLET STREQUAL "wasm32-emscripten" AND NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
    include(FindEmscriptenToolchain)
  endif()

  set(VCPKG_TOOLCHAIN_PATH
    "scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "General path to vcpkg toolchain file"
  )

  # use local vcpkg
  if(EXISTS "$ENV{VCPKG_ROOT}/${VCPKG_TOOLCHAIN_PATH}")
    message(STATUS "Using system-provided vcpkg to manage dependencies")
    set(CMAKE_TOOLCHAIN_FILE
      "$ENV{VCPKG_ROOT}/${VCPKG_TOOLCHAIN_PATH}"
      CACHE STRING "vcpkg toolchain file"
    )
  else()
    message(STATUS "Using bootstrapped vcpkg to manage dependencies")
    set(X_VCPKG_CLONE_DIR "${CMAKE_SOURCE_DIR}/tools/vcpkg/vcpkg"
      CACHE PATH "vcpkg clone directory"
    )
    set(CMAKE_TOOLCHAIN_FILE
      "${X_VCPKG_CLONE_DIR}/${VCPKG_TOOLCHAIN_PATH}"
      CACHE STRING "vcpkg toolchain file"
    )
    include(BootstrapVcpkg)
  endif()

  set(VCPKG_BOOTSTRAP_OPTIONS "-disableMetrics")
  set(VCPKG_INSTALL_OPTIONS "--no-print-usage")
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)
else()
  message(STATUS "Using system-provided dependencies")
endif()

project(openliero
  DESCRIPTION "A brutal subterranean shooter game"
  HOMEPAGE_URL https://github.com/openliero/openliero
  LANGUAGES C CXX
)

# clang-tidy integration
#   `clang-tidy` target: runs scripts/clang-tidy-diff.sh, which is the
#       same check CI runs on PRs (changed lines vs origin/master).
#   OPENLIERO_ENABLE_CLANG_TIDY=ON: wire CMAKE_*_CLANG_TIDY so every
#       compiled TU is linted inline. Slow; off by default.
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

if(CLANG_TIDY_EXE)
  add_custom_target(clang-tidy
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang-tidy-diff.sh"
            "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running clang-tidy on diff vs origin/master"
  )

  include(ProcessorCount)
  ProcessorCount(CLANG_TIDY_JOBS)
  if(CLANG_TIDY_JOBS EQUAL 0)
    set(CLANG_TIDY_JOBS 4)
  endif()

  # Full-tree scan via scripts/clang-tidy-all.sh, matching what CI runs.
  add_custom_target(clang-tidy-all
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang-tidy-all.sh" "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running clang-tidy on all sources under src/"
  )
endif()

if(OPENLIERO_ENABLE_CLANG_TIDY)
  if(NOT CLANG_TIDY_EXE)
    message(FATAL_ERROR "OPENLIERO_ENABLE_CLANG_TIDY=ON but clang-tidy was not found")
  endif()
  set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

# clang-format integration
#   `clang-format` target: runs scripts/clang-format-diff.sh, the same
#       check CI runs on PRs (changed lines vs origin/master).
#   `clang-format-all` target: runs scripts/clang-format-all.sh, a
#       check-only dry run across every source under src/. Useful for
#       triaging existing formatting drift; does not modify files.
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if(CLANG_FORMAT_EXE)
  add_custom_target(clang-format
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang-format-diff.sh"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running clang-format on diff vs origin/master"
  )

  add_custom_target(clang-format-all
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang-format-all.sh"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running clang-format on all sources under src/"
  )
endif()

# find dependencies
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)
find_package(SDL3_image REQUIRED CONFIG)
find_package(miniz CONFIG REQUIRED)
find_package(enet CONFIG REQUIRED PATHS "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/cmake/enet")
find_package(LibJuice CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)

# TODO get rid of this kludgy hack
include_directories(
  src
  src/game
)

# embed version metadata in the code
configure_file(
  "${PROJECT_SOURCE_DIR}/tools/cmake/metadata.cpp.in"
  "${PROJECT_SOURCE_DIR}/src/game/metadata.cpp"
)

if(WIN32)
  configure_file(
    "${PROJECT_SOURCE_DIR}/packaging/icons/openliero.rc.in"
    "${CMAKE_BINARY_DIR}/openliero.rc"
    @ONLY
  )
  configure_file(
    "${PROJECT_SOURCE_DIR}/dist/windows/AppxManifest.xml.in"
    "${CMAKE_BINARY_DIR}/AppxManifest.xml"
    @ONLY
  )
endif()

set(SOURCES
  src/game/bobject.cpp
  src/game/bonus.cpp
  src/game/common.cpp
  src/game/console.cpp
  src/game/cp437.cpp
  src/game/constants.cpp
  src/game/filesystem.cpp
  src/game/game.cpp
  src/game/level.cpp
  src/game/math.cpp
  src/game/ninjarope.cpp
  src/game/nobject.cpp
  src/game/settings.cpp
  src/game/sobject.cpp
  src/game/spectatorviewport.cpp
  src/game/stats_recorder.cpp
  src/game/text.cpp
  src/game/viewport.cpp
  src/game/weapon.cpp
  src/game/worm.cpp
  src/game/ai/dijkstra.cpp
  src/game/ai/predictive_ai.cpp
  src/game/gfx/blit.cpp
  src/game/gfx/font.cpp
  src/game/gfx/palette.cpp
  src/game/gfx/renderer.cpp
  src/game/gfx/sprite.cpp
  src/game/mixer/mixer.cpp
  src/game/mixer/player.cpp
  src/game/metadata.cpp
)

set(GAME_SOURCES ${SOURCES}
  src/game/gfx.cpp
  src/game/keys.cpp
  src/game/gameEntry.cpp
  src/game/mainMenuState.cpp
  src/game/fileSelectorState.cpp
  src/game/weaponMenuState.cpp
  src/game/inputState.cpp
  src/game/gamePlayState.cpp
  src/game/replay.cpp
  src/game/weapsel.cpp
  src/game/controller/commonController.cpp
  src/game/controller/localController.cpp
  src/game/controller/rollbackController.cpp
  src/game/controller/replayController.cpp
  src/game/net/transport.cpp
  src/game/net/session.cpp
  src/game/net/tcArchive.cpp
  src/game/net/memoryFs.cpp
  src/game/net/localaddr.cpp
  src/game/net/stun.cpp
  src/game/net/signaling.cpp
  src/game/net/iceAgent.cpp
  src/game/net/iceBridge.cpp

  src/game/netConnectState.cpp
  src/game/onlineConnectState.cpp
  src/game/statsState.cpp
  src/game/rematchState.cpp
  src/game/menu/booleanSwitchBehavior.cpp
  src/game/menu/enumBehavior.cpp
  src/game/menu/hiddenMenu.cpp
  src/game/menu/integerBehavior.cpp
  src/game/menu/mainMenu.cpp
  src/game/menu/menu.cpp
  src/game/menu/menuItem.cpp
  src/game/menu/timeBehavior.cpp
)

# game
add_library(game ${GAME_SOURCES})
if(WIN32)
  target_compile_definitions(game PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_link_libraries(game
  PUBLIC
  miniz::miniz
  SDL3::SDL3
  SDL3_image::SDL3_image
  enet::enet_static
  LibJuice::LibJuice
  tomlplusplus::tomlplusplus
  xxHash::xxhash
)

if(APPLE)
  add_executable(openliero src/game/main.cpp)
# Commented out code creates a proper bundle, but the app does not run for ... unknown reasons?
# Anyway, enjoy your weird CLI app, macOS users. :(
#  # https://stackoverflow.com/questions/41121000/cmake-os-x-bundle-recursively-copy-directory-to-resources/65519828#65519828
#  # CC-BY-SA-4.0
#  function(resource VAR SOURCE_PATH DESTINATION PATTERN)
#    file(GLOB_RECURSE _LIST CONFIGURE_DEPENDS ${SOURCE_PATH}/${PATTERN})
#    foreach (RESOURCE ${_LIST})
#      get_filename_component(_PARENT ${RESOURCE} DIRECTORY)
#      if (${_PARENT} STREQUAL ${SOURCE_PATH})
#        set(_DESTINATION ${DESTINATION})
#      else ()
#        file(RELATIVE_PATH _DESTINATION ${SOURCE_PATH} ${_PARENT})
#        set(_DESTINATION ${DESTINATION}/${_DESTINATION})
#      endif ()
#      set_property(SOURCE ${RESOURCE} PROPERTY MACOSX_PACKAGE_LOCATION ${_DESTINATION})
#    endforeach (RESOURCE)
#    set(${VAR} ${_LIST} PARENT_SCOPE)
#  endfunction()
#
#  resource(PROFILES "${CMAKE_CURRENT_SOURCE_DIR}/data/Profiles" Resources/Profiles *.lpf)
#  resource(RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/data/Resources" Resources/Resources *.png)
#  resource(SETUPS "${CMAKE_CURRENT_SOURCE_DIR}/data/Setups" Resources/Setups *.cfg)
#  resource(TC "${CMAKE_CURRENT_SOURCE_DIR}/data/TC" Resources/TC *)
#  resource(ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons" Resources openliero.icns)
#
#  set(RESOURCE_FILES ${PROFILES} ${RESOURCES} ${SETUPS} ${TC} ${ICON})
#
#  add_executable(openliero MACOSX_BUNDLE src/game/main.cpp "${RESOURCE_FILES}")
elseif(WIN32)
  add_executable(openliero WIN32 src/game/main.cpp "${CMAKE_BINARY_DIR}/openliero.rc")
elseif(EMSCRIPTEN)
  add_executable(openliero src/game/main.cpp)
  target_link_options(openliero PRIVATE
    -sALLOW_MEMORY_GROWTH=1
    -sMAX_WEBGL_VERSION=2
    -sEXPORTED_RUNTIME_METHODS=FS
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/data/Profiles@/openliero/Profiles"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/data/Resources@/openliero/Resources"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/data/Setups@/openliero/Setups"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/data/TC@/openliero/TC"
  )
  set_target_properties(openliero PROPERTIES SUFFIX ".html")
  target_compile_definitions(game PUBLIC OPENLIERO_EMSCRIPTEN=1)
else()
  add_executable(openliero src/game/main.cpp)
endif()

target_link_libraries(openliero PRIVATE game)

if(NOT EMSCRIPTEN)
  target_compile_definitions(game PRIVATE
    OPENLIERO_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/openliero")
endif()

if(OPENLIERO_BUILD_TCTOOL)
  set(TC_LIB_SOURCES ${SOURCES}
    src/tc_tool/common_exereader.cpp
    src/tc_tool/common_writer.cpp
  )

  add_library(tc ${TC_LIB_SOURCES})

  target_link_libraries(tc
    PUBLIC
    miniz::miniz
    SDL3::SDL3
    xxHash::xxhash
  )

  add_executable(tctool src/tc_tool/tc_tool_main.cpp)
  target_link_libraries(tctool PRIVATE tc)
  if(NOT EMSCRIPTEN)
    target_compile_definitions(tc PRIVATE
      OPENLIERO_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/openliero")
  endif()
endif()

if(OPENLIERO_BUILD_VIDEOTOOL)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswresample
    libswscale
  )

  set(VIDEO_TOOL_SOURCES ${SOURCES}
    src/game/replay.cpp
    src/video_tool/replay_to_video.cpp
    src/video_tool/tools_main.cpp
    src/video_tool/video_recorder.c
  )
  add_library(vt ${VIDEO_TOOL_SOURCES})

  target_link_libraries(vt
    PUBLIC
    miniz::miniz
    SDL3::SDL3
    xxHash::xxhash
    PkgConfig::FFMPEG
  )

  add_executable(videotool ${VIDEO_TOOL_SOURCES})
  target_link_libraries(videotool PRIVATE vt)
endif()

# binaries
# Windows (zip + MSIX) and Emscripten expect a flat install; on Linux/macOS
# we follow the FHS: binaries in bin/, stock data in share/openliero/.
if(WIN32 OR EMSCRIPTEN)
  set(OPENLIERO_INSTALL_BINDIR .)
  set(OPENLIERO_INSTALL_DATADIR .)
else()
  set(OPENLIERO_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
  set(OPENLIERO_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR}/openliero)
endif()

install(TARGETS openliero DESTINATION ${OPENLIERO_INSTALL_BINDIR})
if(OPENLIERO_BUILD_TCTOOL)
  install(TARGETS tctool DESTINATION ${OPENLIERO_INSTALL_BINDIR})
endif()
if(OPENLIERO_BUILD_VIDEOTOOL)
  install(TARGETS videotool DESTINATION ${OPENLIERO_INSTALL_BINDIR})
endif()

if(EMSCRIPTEN)
  # Emscripten produces companion .js and .wasm files alongside the .html
  install(FILES
    $<TARGET_FILE_DIR:openliero>/openliero.js
    $<TARGET_FILE_DIR:openliero>/openliero.wasm
    $<TARGET_FILE_DIR:openliero>/openliero.data
    DESTINATION .
  )
else()
  # assets
  install(DIRECTORY data/Profiles  DESTINATION ${OPENLIERO_INSTALL_DATADIR})
  install(DIRECTORY data/Resources DESTINATION ${OPENLIERO_INSTALL_DATADIR})
  install(DIRECTORY data/Setups    DESTINATION ${OPENLIERO_INSTALL_DATADIR})
  install(DIRECTORY data/TC        DESTINATION ${OPENLIERO_INSTALL_DATADIR})

  if(NOT WIN32)
    install(FILES packaging/openliero.desktop
      RENAME io.github.openliero.openliero.desktop
      DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/openliero.metainfo.xml
      RENAME io.github.openliero.openliero.metainfo.xml
      DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES packaging/icons/openliero-512.png
      RENAME io.github.openliero.openliero.png
      DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
  endif()
endif()

# tests
option(OPENLIERO_BUILD_TESTS "Build tests" OFF)
if(OPENLIERO_BUILD_TESTS)
  find_package(Catch2 3 REQUIRED)
  include(CTest)
  include(Catch)

  add_executable(test_determinism src/tests/test_determinism.cpp)
  target_link_libraries(test_determinism PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_determinism
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_transport src/tests/test_transport.cpp)
  target_link_libraries(test_transport PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_transport DISCOVERY_MODE PRE_TEST)

  add_executable(test_session src/tests/test_session.cpp)
  target_link_libraries(test_session PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_session
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_stun src/tests/test_stun.cpp)
  target_link_libraries(test_stun PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_stun DISCOVERY_MODE PRE_TEST)

  add_executable(test_ice src/tests/test_ice.cpp)
  target_link_libraries(test_ice PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_ice DISCOVERY_MODE PRE_TEST)

  add_executable(test_cp437 src/tests/test_cp437.cpp)
  target_link_libraries(test_cp437 PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_cp437 DISCOVERY_MODE PRE_TEST)

  add_executable(test_io src/tests/test_io.cpp)
  target_link_libraries(test_io PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_io DISCOVERY_MODE PRE_TEST)

  add_executable(test_palette src/tests/test_palette.cpp)
  target_link_libraries(test_palette PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_palette DISCOVERY_MODE PRE_TEST)

  add_executable(test_blit src/tests/test_blit.cpp)
  target_link_libraries(test_blit PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_blit DISCOVERY_MODE PRE_TEST)

  # Headless replay frame hasher (not a Catch2 test): prints per-frame
  # hashes of the composed screen so renderer refactors can be verified
  # pixel-identical against a baseline build. See src/tests/framehash_main.cpp.
  add_executable(framehash src/tests/framehash_main.cpp)
  target_link_libraries(framehash PRIVATE game)

  add_executable(test_paths src/tests/test_paths.cpp)
  target_link_libraries(test_paths PRIVATE game Catch2::Catch2WithMain)
  target_compile_definitions(test_paths PRIVATE
    OPENLIERO_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
  catch_discover_tests(test_paths DISCOVERY_MODE PRE_TEST)

  add_executable(test_settings src/tests/test_settings.cpp)
  target_link_libraries(test_settings PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_settings
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_tc_load src/tests/test_tc_load.cpp)
  target_link_libraries(test_tc_load PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_tc_load
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_sound_player src/tests/test_sound_player.cpp)
  target_link_libraries(test_sound_player PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_sound_player
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )


  add_executable(test_toml_archive src/tests/test_toml_archive.cpp)
  target_link_libraries(test_toml_archive PRIVATE
    cereal::cereal
    tomlplusplus::tomlplusplus
    Catch2::Catch2WithMain)
  target_include_directories(test_toml_archive PRIVATE src src/game)
  catch_discover_tests(test_toml_archive DISCOVERY_MODE PRE_TEST)

  add_executable(test_cereal_types src/tests/test_cereal_types.cpp)
  target_link_libraries(test_cereal_types PRIVATE
    game
    cereal::cereal
    tomlplusplus::tomlplusplus
    Catch2::Catch2WithMain)
  catch_discover_tests(test_cereal_types DISCOVERY_MODE PRE_TEST)

  add_executable(test_snapshot_roundtrip src/tests/test_snapshot_roundtrip.cpp)
  target_link_libraries(test_snapshot_roundtrip PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_snapshot_roundtrip
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_snapshot_fast src/tests/test_snapshot_fast.cpp)
  target_link_libraries(test_snapshot_fast PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_snapshot_fast
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_buffer src/tests/test_rollback_buffer.cpp)
  target_link_libraries(test_rollback_buffer PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_buffer DISCOVERY_MODE PRE_TEST)

  add_executable(test_speculative_suppression src/tests/test_speculative_suppression.cpp)
  target_link_libraries(test_speculative_suppression PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_speculative_suppression
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_prediction_no_rollback src/tests/test_prediction_no_rollback.cpp)
  target_link_libraries(test_prediction_no_rollback PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_prediction_no_rollback
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_correctness src/tests/test_rollback_correctness.cpp)
  target_link_libraries(test_rollback_correctness PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_correctness
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_packet_loss src/tests/test_rollback_packet_loss.cpp)
  target_link_libraries(test_rollback_packet_loss PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_packet_loss
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_reorder src/tests/test_rollback_reorder.cpp)
  target_link_libraries(test_rollback_reorder PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_reorder
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_frame_advantage src/tests/test_frame_advantage.cpp)
  target_link_libraries(test_frame_advantage PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_frame_advantage
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_desync src/tests/test_rollback_desync.cpp)
  target_link_libraries(test_rollback_desync PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_desync
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_generation_drop
    src/tests/test_rollback_generation_drop.cpp)
  target_link_libraries(test_rollback_generation_drop
    PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_generation_drop
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_generation_future
    src/tests/test_rollback_generation_future.cpp)
  target_link_libraries(test_rollback_generation_future
    PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_generation_future
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_skew_repro
    src/tests/test_rollback_skew_repro.cpp)
  target_link_libraries(test_rollback_skew_repro
    PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_skew_repro
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_session_rollback src/tests/test_session_rollback.cpp)
  target_link_libraries(test_session_rollback PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_session_rollback
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_weapsel src/tests/test_rollback_weapsel.cpp)
  target_link_libraries(test_rollback_weapsel PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_weapsel
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_rollback_replay src/tests/test_rollback_replay.cpp)
  target_link_libraries(test_rollback_replay PRIVATE game Catch2::Catch2WithMain)
  catch_discover_tests(test_rollback_replay
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  add_executable(test_versioning src/tests/test_versioning.cpp)
  target_link_libraries(test_versioning PRIVATE
    game
    cereal::cereal
    tomlplusplus::tomlplusplus
    Catch2::Catch2WithMain)
  catch_discover_tests(test_versioning DISCOVERY_MODE PRE_TEST)

  if(OPENLIERO_BUILD_TCTOOL)
    add_executable(test_sfx_loader src/tests/test_sfx_loader.cpp)
    target_link_libraries(test_sfx_loader PRIVATE tc Catch2::Catch2WithMain)
    target_include_directories(test_sfx_loader PRIVATE src src/game)
    catch_discover_tests(test_sfx_loader DISCOVERY_MODE PRE_TEST)
  endif()
endif()

# documentation
install(FILES README.md DESTINATION ${OPENLIERO_INSTALL_DATADIR})

# licenses
install(FILES LICENSE DESTINATION LICENSES)
install(FILES data/TC/openliero/sounds/LICENSE RENAME LICENSE.SOUNDS DESTINATION LICENSES)
