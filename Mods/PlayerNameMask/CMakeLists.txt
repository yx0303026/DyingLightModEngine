if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(PlayerNameMask LANGUAGES CXX)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "PlayerNameMask requires Windows")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/DyingLightOutputDirectories.cmake")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "PlayerNameMask is bound to the x64 game binary")
endif()

add_library(PlayerNameMask SHARED
    "src/PlayerNameMask.cpp"
    "src/PlayerNameMaskRuntime.cpp"
    "include/PlayerNameMaskRuntime.h"
    "../../DyingLightModEngine/include/sdk/EngineAbiTypes.h"
    "../../DyingLightModEngine/include/sdk/EngineHookAbi.h"
    "../../DyingLightModEngine/include/sdk/EngineHostAbi.h"
    "../../DyingLightModEngine/include/sdk/EngineModLifecycleAbi.h"
    "../../DyingLightModEngine/include/logging/EngineLogger.h"
    "exports/x64.def"
)

target_compile_features(PlayerNameMask PRIVATE cxx_std_20)
target_compile_definitions(PlayerNameMask PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(PlayerNameMask PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../DyingLightModEngine/include"
)
target_compile_options(PlayerNameMask PRIVATE /W4 /permissive- /EHsc /utf-8)
set_target_properties(PlayerNameMask PROPERTIES OUTPUT_NAME "player_name_mask")

dlmod_set_output_directories(PlayerNameMask PlayerNameMask)

install(TARGETS PlayerNameMask
    RUNTIME DESTINATION "Mods/PlayerNameMask"
)
