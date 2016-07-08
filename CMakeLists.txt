cmake_minimum_required (VERSION 2.6)
project (openliero)

# add current path as module path to get required config file for sdl 2
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ".")
Find_Package (SDL2 REQUIRED)
include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/ffmpeg)
link_directories(${PROJECT_SOURCE_DIR}/ffmpeg/libavcodec ${PROJECT_SOURCE_DIR}/ffmpeg/libavformat ${PROJECT_SOURCE_DIR}/ffmpeg/libavutil ${PROJECT_SOURCE_DIR}/ffmpeg/libswresample ${PROJECT_SOURCE_DIR}/ffmpeg/libswscale ${PROJECT_SOURCE_DIR}/x264)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s")

set(SOURCES
    game/bobject.cpp
    game/bonus.cpp
    game/common.cpp
    game/console.cpp
    game/constants.cpp
    game/filesystem.cpp
    game/game.cpp
    game/level.cpp
    game/math.cpp
    game/ninjarope.cpp
    game/nobject.cpp
    game/rand.cpp
    game/settings.cpp
    game/sobject.cpp
    game/spectatorviewport.cpp
    game/stats_recorder.cpp
    game/sys.cpp
    game/text.cpp
    game/viewport.cpp
    game/weapon.cpp
    game/worm.cpp
    game/mixer/mixer.c
    game/mixer/player.cpp
    game/gfx/blit.cpp
    game/gfx/font.cpp
    game/gfx/palette.cpp
    game/gfx/renderer.cpp
    game/gfx/sprite.cpp
    gvl/zlib/miniz.c
    gvl/system/system.c
    gvl/support/bits.c
    gvl/support/debug.cpp
    gvl/support/platform.cpp
    gvl/support/profile.cpp
    gvl/io2/stream.cpp
    gvl/io2/convert.cpp
    gvl/containers/list.cpp
    )

set(GAME_SOURCES ${SOURCES}
    game/gfx.cpp
    game/keys.cpp
    game/main.cpp
    game/replay.cpp
    game/sfx.cpp
    game/weapsel.cpp
    game/ai/dijkstra.cpp
    game/ai/predictive_ai.cpp
    game/controller/commonController.cpp
    game/controller/localController.cpp
    game/controller/replayController.cpp
    game/controller/stats_presenter.cpp
    game/menu/booleanSwitchBehavior.cpp
    game/menu/enumBehavior.cpp
    game/menu/hiddenMenu.cpp
    game/menu/integerBehavior.cpp
    game/menu/itemBehavior.cpp
    game/menu/mainMenu.cpp
    game/menu/menu.cpp
    game/menu/menuItem.cpp
    game/menu/timeBehavior.cpp
    )

set(TC_TOOL_SOURCES ${SOURCES}
    tc_tool/common_exereader.cpp
    tc_tool/common_writer.cpp
    tc_tool/tc_tool_main.cpp
    )

set(VIDEO_TOOL_SOURCES ${SOURCES}
    game/replay.cpp
    video_tool/replay_to_video.cpp
    video_tool/tools_main.cpp
    video_tool/video_recorder.c
    )

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(GAME_SOURCES ${GAME_SOURCES} game/SDLmain.m)
    set_source_files_properties(game/SDLmain.m PROPERTIES COMPILE_FLAGS "-std=c99")

    # Populate RESOURCE_FILES with contents of pkg/ directory.
    file(GLOB_RECURSE RESOURCE_FILES pkg/*)
else(APPLE)
    set(GAME_SOURCES ${GAME_SOURCES} game/sdlmain.cpp)
endif(APPLE)

add_executable(openliero ${GAME_SOURCES} ${RESOURCE_FILES})
add_executable(tctool ${TC_TOOL_SOURCES})
add_executable(videotool EXCLUDE_FROM_ALL ${VIDEO_TOOL_SOURCES})
target_link_libraries (openliero ${SDL2_LIBRARY})
target_link_libraries (tctool ${SDL2_LIBRARY})
target_link_libraries (videotool avcodec avformat avutil dl pthread swresample swscale x264)
include_directories(game)

if(APPLE)
    set_target_properties(openliero PROPERTIES MACOSX_BUNDLE TRUE)
    set_target_properties(openliero PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in")

    # Preserve directory structure when copying contents of pkg/ to the .app Resources/ directory.
    foreach(absolute_path ${RESOURCE_FILES})
        file(RELATIVE_PATH relative_path "${CMAKE_CURRENT_SOURCE_DIR}/pkg" ${absolute_path})
        get_filename_component(relative_path ${relative_path} DIRECTORY)
        set_source_files_properties(${absolute_path}
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${relative_path}")
    endforeach(absolute_path)
endif(APPLE)
