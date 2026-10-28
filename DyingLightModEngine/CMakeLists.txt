if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(DyingLightModEngine LANGUAGES CXX)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "DyingLightModEngine requires Windows")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/DyingLightOutputDirectories.cmake")

include(FetchContent)

FetchContent_Declare(
    DearImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.93.0
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    MinHook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.4
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(DearImGui)
if(NOT dearimgui_POPULATED)
    FetchContent_Populate(DearImGui)
endif()
FetchContent_MakeAvailable(MinHook)

add_library(DearImGui STATIC
    "${dearimgui_SOURCE_DIR}/imgui.cpp"
    "${dearimgui_SOURCE_DIR}/imgui_draw.cpp"
    "${dearimgui_SOURCE_DIR}/imgui_tables.cpp"
    "${dearimgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${dearimgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
    "${dearimgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
)
target_include_directories(DearImGui PUBLIC
    "${dearimgui_SOURCE_DIR}"
    "${dearimgui_SOURCE_DIR}/backends"
    "${CMAKE_CURRENT_LIST_DIR}/imgui/include"
)
target_compile_features(DearImGui PUBLIC cxx_std_11)
target_link_libraries(DearImGui PUBLIC d3d11 dxgi user32 imm32)

add_library(DyingLightModEngineImGui STATIC
    "imgui/src/ui.cpp"
    "imgui/src/layout.cpp"
    "imgui/src/controls.cpp"
    "imgui/src/navigation.cpp"
    "imgui/src/hook_runtime.cpp"
    "imgui/src/dx11_hook.cpp"
    "imgui/src/dx12_hook.cpp"
    "imgui/src/opengl_hook.cpp"
    "imgui/src/vulkan_hook.cpp"
    "imgui/src/vuk_hook.cpp"
    "imgui/include/dl_imgui/ui.h"
    "imgui/include/dl_imgui/layout.h"
    "imgui/include/dl_imgui/controls.h"
    "imgui/include/dl_imgui/navigation.h"
    "imgui/include/dl_imgui/imgui.h"
    "imgui/include/dl_imgui/render_hook.h"
    "imgui/include/dl_imgui/dx11_hook.h"
    "imgui/include/dl_imgui/dx12_hook.h"
    "imgui/include/dl_imgui/opengl_hook.h"
    "imgui/include/dl_imgui/vulkan_hook.h"
    "imgui/include/dl_imgui/vuk_hook.h"
    "imgui/include/dl_imgui/hooks.h"
    "imgui/include/imconfig.h"
)
add_library(DyingLightModEngine::ImGui ALIAS DyingLightModEngineImGui)
target_compile_features(DyingLightModEngineImGui PUBLIC cxx_std_20)
target_include_directories(DyingLightModEngineImGui
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/imgui/include"
        "${dearimgui_SOURCE_DIR}"
)
target_compile_options(DyingLightModEngineImGui PRIVATE /W4 /permissive- /EHsc)
target_link_libraries(DyingLightModEngineImGui PUBLIC DearImGui PRIVATE minhook)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DLMOD_ENGINE_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x64.def")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(DLMOD_ENGINE_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x86.def")
else()
    message(FATAL_ERROR "DyingLightModEngine supports only x86 and x64 Windows targets")
endif()

add_library(DyingLightModEngine SHARED
    "src/ModEngineEntry.cpp"
    "src/ModEngineHost.cpp"
    "src/ModEngineApi.cpp"
    "src/EngineLogger.cpp"
    "src/EngineModManager.cpp"
    "src/EngineGraphicsRuntime.cpp"
    "include/EngineHost.h"
    "include/EngineAbi.h"
    "include/EngineLogger.h"
    "include/EngineModManager.h"
    "include/EngineGraphicsRuntime.h"
    "${DLMOD_ENGINE_DEF}"
)

target_compile_features(DyingLightModEngine PRIVATE cxx_std_20)
target_compile_definitions(DyingLightModEngine PRIVATE MODSDK_BUILD WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(DyingLightModEngine
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
)
target_compile_options(DyingLightModEngine PRIVATE /W4 /permissive- /EHsc)
target_link_libraries(DyingLightModEngine PRIVATE DyingLightModEngineImGui d3d11 dxgi)
set_target_properties(DyingLightModEngine PROPERTIES OUTPUT_NAME "DyingLightModEngine")

dlmod_set_output_directories(DyingLightModEngine)
