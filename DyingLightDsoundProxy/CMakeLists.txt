if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(DyingLightDsoundProxy LANGUAGES CXX)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "DyingLightDsoundProxy requires Windows")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/DyingLightOutputDirectories.cmake")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DLMOD_DSOUND_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x64.def")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(DLMOD_DSOUND_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x86.def")
else()
    message(FATAL_ERROR "DyingLightDsoundProxy supports only x86 and x64 Windows targets")
endif()

add_library(DyingLightDsoundProxy SHARED
    "src/dsound_proxy.cpp"
    "src/dsound_proxy_entry.cpp"
    "src/dsound_proxy_runtime.cpp"
    "src/dsound_proxy_runtime.h"
    "${DLMOD_DSOUND_DEF}"
)

target_compile_features(DyingLightDsoundProxy PRIVATE cxx_std_20)
target_compile_definitions(DyingLightDsoundProxy PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX DIRECTSOUND_VERSION=0x0900)
target_compile_options(DyingLightDsoundProxy PRIVATE /W4 /permissive- /EHsc)
target_link_options(DyingLightDsoundProxy PRIVATE /IGNORE:4222)
set_target_properties(DyingLightDsoundProxy PROPERTIES OUTPUT_NAME "dsound")

if(TARGET DyingLightModLoader)
    add_dependencies(DyingLightDsoundProxy DyingLightModLoader)
endif()

dlmod_set_output_directories(DyingLightDsoundProxy)
