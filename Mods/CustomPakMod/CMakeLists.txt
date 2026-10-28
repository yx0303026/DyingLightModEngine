if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(CustomPakMod LANGUAGES CXX)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "CustomPakMod requires Windows")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/DyingLightOutputDirectories.cmake")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CUSTOM_PAK_MOD_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x64.def")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CUSTOM_PAK_MOD_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x86.def")
else()
    message(FATAL_ERROR "CustomPakMod supports only x86 and x64 Windows targets")
endif()

add_library(CustomPakMod SHARED
    "src/CustomPakMod.cpp"
    "src/PakLoader.cpp"
    "include/PakLoader.h"
    "include/PakLoaderAbi.h"
    "../../DyingLightModEngine/include/EngineAbi.h"
    "${CUSTOM_PAK_MOD_DEF}"
)

target_compile_features(CustomPakMod PRIVATE cxx_std_20)
target_compile_definitions(CustomPakMod PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(CustomPakMod PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../DyingLightModEngine/include"
)
target_compile_options(CustomPakMod PRIVATE /W4 /permissive- /EHsc)
set_target_properties(CustomPakMod PROPERTIES OUTPUT_NAME "custom_pak_mod")

dlmod_set_output_directories(CustomPakMod CustomPakMod)

install(TARGETS CustomPakMod
    RUNTIME DESTINATION "Mods/CustomPakMod"
)
install(DIRECTORY "CustomPak/"
    DESTINATION "Mods/CustomPakMod/CustomPak"
)
