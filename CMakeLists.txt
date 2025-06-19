cmake_minimum_required(VERSION 3.10)
project(DirectX11Practice)

set(CMAKE_CXX_STANDARD 17)

add_executable(DirectX11Practice main.cpp)

target_link_libraries(DirectX11Practice d3d11 dxgi dxguid d3dcompiler)

set_target_properties(DirectX11Practice PROPERTIES WIN32_EXECUTABLE YES) 