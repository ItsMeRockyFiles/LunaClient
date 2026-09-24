cmake_minimum_required(VERSION 3.20)
project(LunaLauncher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── ImGui sources ──────────────────────────────────────────────────────────────
# Place Dear ImGui in: <root>/vendor/imgui/
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/../vendor/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_executable(LunaLauncher WIN32
    src/main.cpp
    src/injector.cpp
    src/pipe_client.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(LunaLauncher PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../shared
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(LunaLauncher PRIVATE
    d3d11
    dxgi
    d3dcompiler
    dwmapi
    comdlg32
    shell32
)

target_compile_definitions(LunaLauncher PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    LUNA_VERSION="1.0.0"
    LUNA_NAME="LunaClient"
)

target_compile_options(LunaLauncher PRIVATE
    /O2
    /EHa
)

set_target_properties(LunaLauncher PROPERTIES
    OUTPUT_NAME "LunaClient"
    WIN32_EXECUTABLE TRUE
)
