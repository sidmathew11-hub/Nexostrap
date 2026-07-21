cmake_minimum_required(VERSION 3.20)
project(NexoExecutor VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find DirectX
find_library(D3D11_LIB d3d11)
find_library(DXGI_LIB dxgi)

# ImGui directory (user must clone docking branch)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui CACHE PATH "ImGui source directory")

# Check ImGui exists
if(NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    message(FATAL_ERROR "ImGui not found at ${IMGUI_DIR}. \
        Please clone the docking branch:\n\
        git clone --branch docking https://github.com/ocornut/imgui.git ${IMGUI_DIR}")
endif()

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

# nlohmann/json (header only)
set(JSON_DIR ${CMAKE_SOURCE_DIR}/third_party/nlohmann)

# ── Nexo Executable ──
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
    ${D3D11_LIB}
    ${DXGI_LIB}
    winhttp
    shell32
    comdlg32
    gdi32
    imm32
)

target_compile_definitions(Nexo PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS
    _CRT_SECURE_NO_WARNINGS
)

# Enable multi-processor compilation
if(MSVC)
    target_compile_options(Nexo PRIVATE /MP /W3 /EHsc)
endif()

# ── NexoCore DLL ──
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

# Copy DLL next to executable
add_custom_command(TARGET NexoCore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:NexoCore>
    $<TARGET_FILE_DIR:Nexo>
)

# Install
install(TARGETS Nexo NexoCore
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
)
