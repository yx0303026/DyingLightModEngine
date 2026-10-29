if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(CheatMod LANGUAGES CXX)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "CheatMod requires Windows")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/DyingLightOutputDirectories.cmake")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CHEAT_MOD_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x64.def")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CHEAT_MOD_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x86.def")
else()
    message(FATAL_ERROR "CheatMod supports only x86 and x64 Windows targets")
endif()

add_library(CheatMod SHARED
    "src/CheatMod.cpp"
    "src/CheatModInput.cpp"
    "src/CheatModLobby.cpp"
    "src/CheatModLobbyView.cpp"
    "src/CheatModRuntime.cpp"
    "src/CheatModUi.cpp"
    "include/CheatModInput.h"
    "include/CheatModLobby.h"
    "include/CheatModLobbyView.h"
    "include/CheatModRuntime.h"
    "include/CheatModUi.h"
    "../../DyingLightModEngine/include/sdk/EngineAbiTypes.h"
    "../../DyingLightModEngine/include/sdk/EngineHostAbi.h"
    "../../DyingLightModEngine/include/sdk/EngineLobbyAbi.h"
    "../../DyingLightModEngine/include/sdk/EngineModLifecycleAbi.h"
    "../../DyingLightModEngine/include/sdk/EngineUiInputAbi.h"
    "../../DyingLightModEngine/include/logging/EngineLogger.h"
    "${CHEAT_MOD_DEF}"
)

target_compile_features(CheatMod PRIVATE cxx_std_20)
target_compile_definitions(CheatMod PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(CheatMod PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../DyingLightModEngine/include"
)
target_compile_options(CheatMod PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(CheatMod PRIVATE DyingLightModEngine::ImGui)
set_target_properties(CheatMod PROPERTIES OUTPUT_NAME "cheat_mod")

dlmod_set_output_directories(CheatMod CheatMod)

install(TARGETS CheatMod
    RUNTIME DESTINATION "Mods/CheatMod"
)
