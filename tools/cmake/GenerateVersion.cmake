find_package(Git)

if(Git_FOUND)
  execute_process(
    COMMAND git describe --tags --dirty=-dev
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE VERSION_GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RESULT_TAG
  )
  if(RESULT_TAG AND NOT RESULT_TAG EQUAL 0)
    set(VERSION_GIT_TAG "0.0.0")
    message(WARNING "Failed to determine version from git describe. Using default version: ${VERSION_GIT_TAG}")
  endif()

  execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE VERSION_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RESULT_HASH
  )
  if(RESULT_HASH AND NOT RESULT_HASH EQUAL 0)
    set(VERSION_GIT_HASH "YNOHASH")
    message(WARNING "Failed to determine commit hash from git rev-parse. Using default commit hash: ${VERSION_GIT_HASH}")
  endif()

  execute_process(
    COMMAND git show --no-patch --format=%cd --date=format:%Y.%m.%d
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE VERSION_GIT_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RESULT_DATE
  )
  if(RESULT_DATE AND NOT RESULT_DATE EQUAL 0)
    set(VERSION_GIT_DATE "1970.01.01")
    message(WARNING "Failed to determine commit date from git show. Using default commit date: ${VERSION_GIT_DATE}")
  endif()

  execute_process(
    COMMAND git show --no-patch --format=%cd --date=format:%H:%M:%S
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE VERSION_GIT_TIME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RESULT_TIME
  )
  if(RESULT_TIME AND NOT RESULT_TIME EQUAL 0)
    set(VERSION_GIT_TIME "00:00:00")
    message(WARNING "Failed to determine commit time from git show. Using default commit time: ${VERSION_GIT_TIME}")
  endif()
else()
  message(WARNING "Git not found. Using default values for VERSION_* variables.")
  set(VERSION_GIT_TAG "0.0.0")
  set(VERSION_GIT_HASH "YNOHASH")
  set(VERSION_GIT_DATE "1970.01.01")
  set(VERSION_GIT_TIME "00:00:00")
endif()

# Parse VERSION_GIT_TAG into integer components for Windows VERSIONINFO.
# Strips leading 'v', extracts MAJOR.MINOR.PATCH and commit-count (build).
# "v1.2.3" → 1,2,3,0 | "v1.2.3-42-gabcdef" → 1,2,3,42 | "0.0.0" → 0,0,0,0
string(REGEX REPLACE "^v" "" _ver_stripped "${VERSION_GIT_TAG}")
if(_ver_stripped MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-([0-9]+))?")
  set(VERSION_MAJOR "${CMAKE_MATCH_1}")
  set(VERSION_MINOR "${CMAKE_MATCH_2}")
  set(VERSION_PATCH "${CMAKE_MATCH_3}")
  if(CMAKE_MATCH_5)
    set(VERSION_BUILD "${CMAKE_MATCH_5}")
  else()
    set(VERSION_BUILD "0")
  endif()
else()
  set(VERSION_MAJOR "0")
  set(VERSION_MINOR "0")
  set(VERSION_PATCH "0")
  set(VERSION_BUILD "0")
endif()
