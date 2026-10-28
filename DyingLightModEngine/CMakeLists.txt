if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(DyingLightModEngine LANGUAGES CXX)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "DyingLightModEngine requires Windows")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/DyingLightOutputDirectories.cmake")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DLMOD_ENGINE_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x64.def")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(DLMOD_ENGINE_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x86.def")
else()
    message(FATAL_ERROR "DyingLightModEngine supports only x86 and x64 Windows targets")
endif()

add_library(DyingLightModEngine SHARED
    "src/dl_mod_engine_entry.cpp"
    "src/dl_mod_engine_host.cpp"
    "src/dl_mod_engine_api.cpp"
    "src/dl_mod_engine_logger.cpp"
    "src/mod_manager.cpp"
    "include/dl_mod_engine_host.h"
    "include/ModSDK.h"
    "include/modsdk_abi.h"
    "include/modsdk_logger.h"
    "include/modsdk_mod_manager.h"
    "${DLMOD_ENGINE_DEF}"
)

target_compile_features(DyingLightModEngine PRIVATE cxx_std_20)
target_compile_definitions(DyingLightModEngine PRIVATE MODSDK_BUILD WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(DyingLightModEngine PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_options(DyingLightModEngine PRIVATE /W4 /permissive- /EHsc)
set_target_properties(DyingLightModEngine PROPERTIES OUTPUT_NAME "DyingLightModEngine")

dlmod_set_output_directories(DyingLightModEngine)
