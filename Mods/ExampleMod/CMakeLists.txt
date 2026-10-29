if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(ExampleMod LANGUAGES CXX)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "ExampleMod requires Windows")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/DyingLightOutputDirectories.cmake")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(EXAMPLE_MOD_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x64.def")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(EXAMPLE_MOD_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x86.def")
else()
    message(FATAL_ERROR "ExampleMod supports only x86 and x64 Windows targets")
endif()

add_library(ExampleMod SHARED
    "src/ExampleMod.cpp"
    "../../DyingLightModEngine/include/sdk/EngineAbi.h"
    "../../DyingLightModEngine/include/logging/EngineLogger.h"
    "${EXAMPLE_MOD_DEF}"
)

target_compile_features(ExampleMod PRIVATE cxx_std_20)
target_compile_definitions(ExampleMod PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(ExampleMod PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../DyingLightModEngine/include"
)
target_compile_options(ExampleMod PRIVATE /W4 /permissive- /EHsc)
set_target_properties(ExampleMod PROPERTIES OUTPUT_NAME "example_mod")

dlmod_set_output_directories(ExampleMod ExampleMod)

install(TARGETS ExampleMod
    RUNTIME DESTINATION "Mods/ExampleMod"
)
