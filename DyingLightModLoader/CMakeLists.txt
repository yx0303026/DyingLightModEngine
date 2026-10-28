if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(DyingLightModLoader LANGUAGES CXX)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "DyingLightModLoader requires Windows")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/DyingLightOutputDirectories.cmake")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DLMOD_LOADER_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x64.def")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(DLMOD_LOADER_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x86.def")
else()
    message(FATAL_ERROR "DyingLightModLoader supports only x86 and x64 Windows targets")
endif()

add_library(DyingLightModLoader SHARED
    "src/host_logger.cpp"
    "src/host_logger.h"
    "src/loader_entry.cpp"
    "src/loader_runtime.cpp"
    "src/mod_engine_host.cpp"
    "src/loader_runtime.h"
    "${DLMOD_LOADER_DEF}"
)

target_compile_features(DyingLightModLoader PRIVATE cxx_std_20)
target_compile_definitions(DyingLightModLoader PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(DyingLightModLoader PRIVATE /W4 /permissive- /EHsc)

if(TARGET ModSDK)
    add_dependencies(DyingLightModLoader ModSDK)
else()
    add_library(ModSDK SHARED IMPORTED GLOBAL)
    set_target_properties(ModSDK PROPERTIES
        IMPORTED_IMPLIB "${CMAKE_CURRENT_LIST_DIR}/../DyingLightModEngine/lib/ModSDK.lib"
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../DyingLightModEngine/bin/ModSDK.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../DyingLightModEngine/include"
    )
endif()

target_link_libraries(DyingLightModLoader PRIVATE ModSDK)
add_custom_command(TARGET DyingLightModLoader POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:ModSDK>"
        "$<TARGET_FILE_DIR:DyingLightModLoader>/ModSDK.dll"
    VERBATIM
)

dlmod_set_output_directories(DyingLightModLoader)
