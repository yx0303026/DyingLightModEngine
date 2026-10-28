if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(ModSDK LANGUAGES CXX)
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

add_library(ModSDK SHARED
    "src/dl_mod_engine_api.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../SDK/include/ModSDK.h"
    "${DLMOD_ENGINE_DEF}"
)

target_compile_features(ModSDK PRIVATE cxx_std_20)
target_compile_definitions(ModSDK PRIVATE MODSDK_BUILD WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(ModSDK PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../SDK/include")
target_compile_options(ModSDK PRIVATE /W4 /permissive- /EHsc)
set_target_properties(ModSDK PROPERTIES OUTPUT_NAME "ModSDK")

dlmod_set_sdk_output_directories(ModSDK)
