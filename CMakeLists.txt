cmake_minimum_required (VERSION 2.6)
project (openliero)

# add current path as module path to get required config file for sdl 2 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ".")
Find_Package (SDL2 REQUIRED)
include_directories("${PROJECT_SOURCE_DIR}")

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s")

set(SOURCES
    bobject.cpp
    bonus.cpp
    common.cpp
    console.cpp
    constants.cpp
    filesystem.cpp
    game.cpp
    level.cpp
    math.cpp
    ninjarope.cpp
    nobject.cpp
    rand.cpp
    settings.cpp
    sobject.cpp
    spectatorviewport.cpp
    stats_recorder.cpp
    sys.cpp
    text.cpp
    viewport.cpp
    weapon.cpp
    worm.cpp
    mixer/mixer.c
    mixer/player.cpp
    gfx/blit.cpp
    gfx/font.cpp
    gfx/palette.cpp
    gfx/renderer.cpp
    gfx/sprite.cpp
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
	gfx.cpp
    keys.cpp
	main.cpp
    replay.cpp
	sfx.cpp
    weapsel.cpp
	ai/dijkstra.cpp
    ai/predictive_ai.cpp
	controller/commonController.cpp
    controller/localController.cpp
    controller/replayController.cpp
    controller/stats_presenter.cpp
	menu/booleanSwitchBehavior.cpp
    menu/enumBehavior.cpp
    menu/hiddenMenu.cpp
    menu/integerBehavior.cpp
    menu/itemBehavior.cpp
    menu/mainMenu.cpp
    menu/menu.cpp
    menu/menuItem.cpp
    menu/timeBehavior.cpp
	)

set(TC_TOOL_SOURCES ${SOURCES}
	common_exereader.cpp
	common_writer.cpp
	tc_tool_main.cpp
	)

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(GAME_SOURCES ${GAME_SOURCES} SDLmain.m)
    set_source_files_properties(SDLmain.m PROPERTIES COMPILE_FLAGS "-std=c99")
else(APPLE)
    set(GAME_SOURCES ${GAME_SOURCES} sdlmain.cpp)
endif(APPLE)

add_executable(openliero ${GAME_SOURCES})
add_executable(tctool ${TC_TOOL_SOURCES})
target_link_libraries (openliero ${SDL2_LIBRARY})
