cmake_minimum_required(VERSION 3.10)
project(DirectX11Practice)

set(CMAKE_CXX_STANDARD 17)

add_executable(DirectX11Practice main.cpp
    extern/imgui/imgui.cpp
    extern/imgui/imgui_draw.cpp
    extern/imgui/imgui_tables.cpp
    extern/imgui/imgui_widgets.cpp
    extern/imgui/backends/imgui_impl_win32.cpp
    extern/imgui/backends/imgui_impl_dx11.cpp
)

target_link_libraries(DirectX11Practice d3d11 dxgi dxguid d3dcompiler)

set_target_properties(DirectX11Practice PROPERTIES WIN32_EXECUTABLE YES)

# ImGui include 디렉터리 추가
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/backends
) 