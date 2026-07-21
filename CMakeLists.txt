cmake_minimum_required(VERSION 3.20)
project(NexoExecutor VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui CACHE PATH "ImGui source directory")

if(NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    message(FATAL_ERROR "ImGui not found. Run: git clone --branch docking https://github.com/ocornut/imgui.git ${IMGUI_DIR}")
endif()

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

set(JSON_DIR ${CMAKE_SOURCE_DIR}/third_party)

add_executable(Nexo WIN32
    Nexo/main.cpp
    Nexo/ui.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(Nexo PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${JSON_DIR}
    Nexo
)

target_link_libraries(Nexo PRIVATE
    d3d11
    dxgi
    winhttp
    shell32
    comdlg32
    gdi32
    imm32
    user32
)

target_compile_definitions(Nexo PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(Nexo PRIVATE /MP /W3 /EHsc)
endif()

add_library(NexoCore SHARED
    NexoCore/dllmain.cpp
)

target_include_directories(NexoCore PRIVATE
    ${JSON_DIR}
    NexoCore
)

target_link_libraries(NexoCore PRIVATE
    winhttp
)

target_compile_definitions(NexoCore PRIVATE
    NEXOCORE_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(NexoCore PRIVATE /MP /W3 /EHsc)
endif()
