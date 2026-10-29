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
FetchContent_MakeAvailable(DearImGui)

function(dlmod_make_min_hook_available)
    # MinHook still declares compatibility with CMake versions below 3.10.
    # Raise its policy baseline without changing the third-party source tree.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    FetchContent_MakeAvailable(MinHook)
endfunction()

dlmod_make_min_hook_available()

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
    "imgui/src/font.cpp"
    "imgui/src/layout.cpp"
    "imgui/src/controls.cpp"
    "imgui/src/dialog.cpp"
    "imgui/src/message_dialog.cpp"
    "imgui/src/custom_dialog.cpp"
    "imgui/src/data_table.cpp"
    "imgui/src/navigation.cpp"
    "imgui/src/style.cpp"
    "imgui/src/hook_runtime.cpp"
    "imgui/src/dx11_hook.cpp"
    "imgui/src/dx12_hook.cpp"
    "imgui/src/opengl_hook.cpp"
    "imgui/src/vulkan_hook.cpp"
    "imgui/src/vuk_hook.cpp"
    "imgui/include/dl_imgui/ui.h"
    "imgui/include/dl_imgui/font.h"
    "imgui/include/dl_imgui/layout.h"
    "imgui/include/dl_imgui/controls.h"
    "imgui/include/dl_imgui/dialog.h"
    "imgui/include/dl_imgui/message_dialog.h"
    "imgui/include/dl_imgui/custom_dialog.h"
    "imgui/include/dl_imgui/data_table.h"
    "imgui/include/dl_imgui/navigation.h"
    "imgui/include/dl_imgui/style.h"
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
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${dearimgui_SOURCE_DIR}"
)
target_compile_options(DyingLightModEngineImGui PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(DyingLightModEngineImGui PUBLIC DearImGui PRIVATE minhook)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DLMOD_ENGINE_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x64.def")
    enable_language(ASM_MASM)
    set(DLMOD_ENTITY_TRACKING_ASM "src/tracking/EngineHunterTracking.asm")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(DLMOD_ENGINE_DEF "${CMAKE_CURRENT_LIST_DIR}/exports/x86.def")
    set(DLMOD_ENTITY_TRACKING_ASM "")
else()
    message(FATAL_ERROR "DyingLightModEngine supports only x86 and x64 Windows targets")
endif()

add_library(DyingLightModEngine SHARED
    "src/core/ModEngineEntry.cpp"
    "src/core/ModEngineHost.cpp"
    "src/core/ModEngineApi.cpp"
    "src/logging/EngineLogger.cpp"
    "src/managers/EngineModManager.cpp"
    "src/runtime/EngineGraphicsRuntime.cpp"
    "src/runtime/EngineInputRuntime.cpp"
    "src/runtime/EngineDirectInputRuntime.cpp"
    "src/runtime/EngineCollisionRuntime.cpp"
    "src/runtime/EngineHookRuntime.cpp"
    "src/runtime/EngineEntityBindings.cpp"
    "src/runtime/EngineEntityRuntime.cpp"
    "src/runtime/EngineLobbyData.cpp"
    "src/runtime/EngineLobbyCollector.cpp"
    "src/runtime/EngineLobbyRuntime.cpp"
    "src/runtime/ui/NativeUiBackend.cpp"
    "src/runtime/ui/PakUiBackend.cpp"
    "src/runtime/ui/EngineUiRuntime.cpp"
    "src/tracking/EngineHunterTrackingRuntime.cpp"
    "${DLMOD_ENTITY_TRACKING_ASM}"
    "include/core/EngineHost.h"
    "include/sdk/EngineAbiTypes.h"
    "include/sdk/EngineApiAbi.h"
    "include/sdk/EngineCollisionAbi.h"
    "include/sdk/EngineEntityAbi.h"
    "include/sdk/EngineHookAbi.h"
    "include/sdk/EngineHostAbi.h"
    "include/sdk/EngineLobbyAbi.h"
    "include/sdk/EngineModLifecycleAbi.h"
    "include/sdk/EngineUiInputAbi.h"
    "include/logging/EngineLogger.h"
    "include/managers/EngineModManager.h"
    "include/runtime/EngineGraphicsRuntime.h"
    "include/runtime/EngineInputRuntime.h"
    "include/runtime/EngineDirectInputRuntime.h"
    "include/runtime/EngineCollisionRuntime.h"
    "include/runtime/EngineHookRuntime.h"
    "include/runtime/EngineEntityBindings.h"
    "include/runtime/EngineEntityRuntime.h"
    "include/runtime/EngineLobbyData.h"
    "include/runtime/EngineLobbyCollector.h"
    "include/runtime/EngineLobbyRuntime.h"
    "include/runtime/ui/EngineUiTypes.h"
    "include/runtime/ui/NativeUiBackend.h"
    "include/runtime/ui/PakUiBackend.h"
    "include/runtime/ui/EngineUiRuntime.h"
    "include/tracking/EngineHunterTrackingRuntime.h"
    "include/tracking/EngineHunterMotionTracker.h"
    "src/tracking/EngineHunterMotionTracker.cpp"
    "${DLMOD_ENGINE_DEF}"
)

target_compile_features(DyingLightModEngine PRIVATE cxx_std_20)
target_compile_definitions(DyingLightModEngine PRIVATE MODSDK_BUILD WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(DyingLightModEngine
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
)
target_compile_options(DyingLightModEngine PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W4>
    $<$<COMPILE_LANGUAGE:CXX>:/permissive->
    $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
    $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
)
target_link_libraries(DyingLightModEngine PRIVATE DyingLightModEngineImGui minhook d3d11 dxgi dxguid)
set_target_properties(DyingLightModEngine PROPERTIES OUTPUT_NAME "DyingLightModEngine")

dlmod_set_output_directories(DyingLightModEngine)
