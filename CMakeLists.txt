cmake_minimum_required(VERSION 3.20)
project(LunaClient_Root)

include(FetchContent)

# ── MinHook (auto-download from GitHub) ───────────────────────────────────────
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(minhook)

# ── LunaClient.dll ────────────────────────────────────────────────────────────
add_library(LunaClient SHARED
    src/dllmain.cpp
    src/roblox/roblox.cpp
    src/hooks/hooks.cpp
    src/pipe/pipe_server.cpp
    src/environment/environment.cpp
    src/environment/functions/filesystem.cpp
    src/environment/functions/closures.cpp
    src/environment/functions/instances.cpp
    src/environment/functions/metatables.cpp
    src/environment/functions/debug_lib.cpp
    src/environment/functions/misc.cpp
    src/environment/functions/http.cpp
)

set_target_properties(LunaClient PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "LunaClient"
    SUFFIX ".dll"
    PREFIX ""
)

target_include_directories(LunaClient PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(LunaClient PRIVATE
    minhook
    winhttp
    ntdll
)

target_compile_definitions(LunaClient PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    LUNA_VERSION="1.0.0"
    LUNA_NAME="LunaClient"
)

target_compile_options(LunaClient PRIVATE
    /O2 /GS- /EHa
)

# ── LunaInjector.exe (CLI tool — spawned by the Node.js backend) ──────────────
add_executable(LunaInjector
    launcher/src/injector.cpp
    launcher/src/injector_cli.cpp
)

set_target_properties(LunaInjector PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "LunaInjector"
)

target_include_directories(LunaInjector PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/shared
)

target_compile_definitions(LunaInjector PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(LunaInjector PRIVATE
    /O2 /EHa
)

# Both targets output to bin/
set_target_properties(LunaClient    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(LunaClient    PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(LunaInjector  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
