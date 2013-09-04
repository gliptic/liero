cmake_minimum_required (VERSION 2.6)
project (openliero)

Find_Package (SDL REQUIRED)
include_directories("${PROJECT_SOURCE_DIR}")

set(CMAKE_CXX_FLAGS "-std=c++11")

set(SOURCES
    bobject.cpp
    bonus.cpp
    common.cpp
    console.cpp
    constants.cpp
    filesystem.cpp
    game.cpp
    gfx.cpp
    keys.cpp
    level.cpp
    main.cpp
    math.cpp
    ninjarope.cpp
    nobject.cpp
    rand.cpp
    reader.cpp
    replay.cpp
    settings.cpp
    sfx.cpp
    sobject.cpp
    stats_recorder.cpp
    sys.cpp
    text.cpp
    viewport.cpp
    weapon.cpp
    weapsel.cpp
    worm.cpp
    mixer/mixer.c
    mixer/player.cpp
    menu/booleanSwitchBehavior.cpp
    menu/enumBehavior.cpp
    menu/hiddenMenu.cpp
    menu/integerBehavior.cpp
    menu/itemBehavior.cpp
    menu/mainMenu.cpp
    menu/menu.cpp
    menu/menuItem.cpp
    menu/timeBehavior.cpp
    controller/commonController.cpp
    controller/localController.cpp
    controller/replayController.cpp
    controller/stats_presenter.cpp
    ai/dijkstra.cpp
    ai/predictive_ai.cpp
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
    gvl/resman/shared.cpp
    gvl/io2/stream.cpp
    gvl/io/convert.cpp
    gvl/io/encoding.cpp
    gvl/io/stream.cpp
    gvl/containers/list.cpp
    )

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(SOURCES ${SOURCES} SDLmain.m)
    set_source_files_properties(SDLmain.m PROPERTIES COMPILE_FLAGS "-std=c99")
endif(APPLE)

add_executable(openliero ${SOURCES})
target_link_libraries (openliero ${SDL_LIBRARY})