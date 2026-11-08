cmake_minimum_required(VERSION 3.20)
project(SteelFrontOnlineDX11 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /W3 /O2)
endif()

add_executable(SteelFrontGame WIN32 src/game_main.cpp)
target_link_libraries(SteelFrontGame PRIVATE d3d11 dxgi d3dcompiler ws2_32 winmm dwmapi gdi32 user32 ole32 shell32)
set_target_properties(SteelFrontGame PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../bin/win64")

add_executable(SteelFrontServer server/server_main.cpp)
target_link_libraries(SteelFrontServer PRIVATE ws2_32)
set_target_properties(SteelFrontServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../bin/win64")

add_executable(SteelFrontLauncher WIN32 launcher/launcher_main.cpp)
target_link_libraries(SteelFrontLauncher PRIVATE d3d11 dxgi d3dcompiler winhttp dwmapi msimg32 gdi32 user32 shell32 ole32)
set_target_properties(SteelFrontLauncher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/..")
