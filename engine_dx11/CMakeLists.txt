cmake_minimum_required(VERSION 3.20)
project(SteelFrontOnlineDagor65 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DAGOR_ROOT "${CMAKE_SOURCE_DIR}/..")
set(QUIRREL_ROOT "${DAGOR_ROOT}/prog/1stPartyLibs/quirrel/quirrel")

file(GLOB_RECURSE QUIRREL_SOURCES
    "${QUIRREL_ROOT}/squirrel/*.cpp"
    "${QUIRREL_ROOT}/sqstdlib/*.cpp"
    "${QUIRREL_ROOT}/helpers/*.cpp"
)
list(FILTER QUIRREL_SOURCES EXCLUDE REGEX "sqstdsystem\\.cpp$")

set(DAGOR_MATH_SOURCES
    "${DAGOR_ROOT}/prog/engine/math/perlin.cpp"
    "${DAGOR_ROOT}/prog/engine/math/rndSeed.cpp"
    "${DAGOR_ROOT}/prog/engine/math/mathAng.cpp"
    "${DAGOR_ROOT}/prog/engine/math/math3d.cpp"
    "${DAGOR_ROOT}/prog/engine/math/wooray3d.cpp"
    "${DAGOR_ROOT}/prog/engine/math/triangleTriangleIntersection.cpp"
)

add_library(dagor_quirrel STATIC ${QUIRREL_SOURCES})
target_include_directories(dagor_quirrel PUBLIC
    "${QUIRREL_ROOT}"
    "${QUIRREL_ROOT}/include"
    "${QUIRREL_ROOT}/internal"
    "${QUIRREL_ROOT}/helpers"
    "${QUIRREL_ROOT}/squirrel"
    "${QUIRREL_ROOT}/squirrel/compiler"
    "${QUIRREL_ROOT}/sqstdlib"
)

add_library(dagor_math STATIC ${DAGOR_MATH_SOURCES})
target_include_directories(dagor_math PUBLIC
    "${DAGOR_ROOT}/prog/dagorInclude"
    "${DAGOR_ROOT}/prog/1stPartyLibs"
)
target_compile_definitions(dagor_math PUBLIC
    _USE_MATH_DEFINES
    _TARGET_PC_WIN=1
    _TARGET_64BIT=1
    _TARGET_SIMD_SSE=4
    _TARGET_STATIC_LIB=1
)
if(MSVC)
    target_compile_options(dagor_math PRIVATE /FIutil/dag_globDef.h)
else()
    target_compile_options(dagor_math PRIVATE -fms-extensions -msse4.1 -include util/dag_globDef.h)
endif()

include_directories(
    "${QUIRREL_ROOT}/include"
    "${DAGOR_ROOT}/prog/dagorInclude"
    "${DAGOR_ROOT}/prog/1stPartyLibs"
    "${DAGOR_ROOT}/prog/engine_dagor_rt"
    "${DAGOR_ROOT}/prog"
)

if(MSVC)
    add_compile_options(/utf-8 /W3 /O2 /D_USE_MATH_DEFINES /DDAGOR_ENABLE_QUIRREL_VM=1)
else()
    add_compile_options(-O2 -fms-extensions -msse4.1 -D_USE_MATH_DEFINES -D_TARGET_PC_WIN=1 -D_TARGET_64BIT=1 -D_TARGET_SIMD_SSE=4 -D_TARGET_STATIC_LIB=1 -DDAGOR_ENABLE_QUIRREL_VM=1)
endif()

add_executable(SteelFrontGame WIN32 src/game_main.cpp)
target_link_libraries(SteelFrontGame PRIVATE dagor_quirrel dagor_math d3d11 dxgi d3dcompiler ws2_32 winmm dwmapi gdi32 user32 ole32 shell32 uuid)
set_target_properties(SteelFrontGame PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${DAGOR_ROOT}/bin/win64")

add_executable(SteelFrontServer server/server_main.cpp)
target_link_libraries(SteelFrontServer PRIVATE dagor_quirrel dagor_math ws2_32 winmm)
set_target_properties(SteelFrontServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${DAGOR_ROOT}/bin/win64")

add_executable(SteelFrontLauncher WIN32 launcher/launcher_main.cpp)
target_link_libraries(SteelFrontLauncher PRIVATE dagor_quirrel dagor_math d3d11 dxgi d3dcompiler winhttp dwmapi msimg32 gdi32 user32 shell32 ole32 ws2_32 uuid)
set_target_properties(SteelFrontLauncher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${DAGOR_ROOT}")
