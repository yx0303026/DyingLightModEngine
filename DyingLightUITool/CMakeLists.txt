if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(DyingLightUITool LANGUAGES CXX)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "DyingLightUITool 需要 Windows")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/DyingLightOutputDirectories.cmake")

add_executable(DyingLightUITool WIN32
    "src/Main.cpp"
    "src/XuiModel.cpp"
    "src/XuiParser.cpp"
    "src/XuiRenderer.cpp"
    "src/XuiEditor.cpp"
    "include/XuiModel.h"
    "include/XuiParser.h"
    "include/XuiRenderer.h"
    "include/XuiEditor.h"
)

target_compile_features(DyingLightUITool PRIVATE cxx_std_20)
target_compile_definitions(DyingLightUITool PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_include_directories(DyingLightUITool PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
)
target_compile_options(DyingLightUITool PRIVATE
    /W4
    /permissive-
    /EHsc
    /utf-8
)
target_link_libraries(DyingLightUITool PRIVATE
    DearImGui
    d3d11
    dxgi
    user32
    gdi32
    imm32
    comdlg32
)

set_target_properties(DyingLightUITool PROPERTIES
    OUTPUT_NAME "DyingLightUITool"
)

dlmod_set_output_directories(DyingLightUITool)

add_executable(DyingLightUIToolTests
    "tests/XuiParserTests.cpp"
    "src/XuiModel.cpp"
    "src/XuiParser.cpp"
    "include/XuiModel.h"
    "include/XuiParser.h"
)
target_compile_features(DyingLightUIToolTests PRIVATE cxx_std_20)
target_compile_definitions(DyingLightUIToolTests PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_include_directories(DyingLightUIToolTests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
)
target_compile_options(DyingLightUIToolTests PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(DyingLightUIToolTests PRIVATE user32)

enable_testing()
add_test(
    NAME DyingLightUIToolParserRoundTrip
    COMMAND DyingLightUIToolTests "${CMAKE_CURRENT_LIST_DIR}/tests/Fixtures/ParserFixture.xui"
)

install(TARGETS DyingLightUITool
    RUNTIME DESTINATION "Tools/DyingLightUITool"
)
