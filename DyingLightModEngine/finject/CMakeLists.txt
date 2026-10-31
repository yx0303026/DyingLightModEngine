# Finject 构建概览：声明独立静态库、公共头文件路径与可选集成测试。

add_library(DyingLightModEngineFinject STATIC
    "src/Runtime.cpp"
    "src/HookHandle.cpp"
    "include/finject/types.h"
    "include/finject/runtime.h"
    "include/finject/hook_handle.h"
    "include/finject/hook_point.h"
    "include/finject/finject.h"
)
add_library(DyingLightModEngine::Finject ALIAS DyingLightModEngineFinject)
target_compile_features(DyingLightModEngineFinject PUBLIC cxx_std_20)
target_include_directories(DyingLightModEngineFinject PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
)
target_compile_definitions(DyingLightModEngineFinject PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(DyingLightModEngineFinject PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(DyingLightModEngineFinject PRIVATE minhook)

option(DLMOD_FINJECT_BUILD_TESTS "构建 Finject Windows 集成测试" OFF)
if(DLMOD_FINJECT_BUILD_TESTS)
    add_executable(DyingLightModEngineFinjectTests "tests/FinjectTests.cpp")
    target_compile_features(DyingLightModEngineFinjectTests PRIVATE cxx_std_20)
    target_compile_options(DyingLightModEngineFinjectTests PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(DyingLightModEngineFinjectTests PRIVATE DyingLightModEngineFinject)
endif()
