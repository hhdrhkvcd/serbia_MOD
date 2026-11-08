@echo off
chcp 65001 >nul
title Сборка Steel Front: Online (C++20 • Gaijin Dagor Engine 6.5 • Windows x64)
cd /d "%~dp0\.."
if not exist "bin\win64" mkdir "bin\win64"
if not exist "game\res" mkdir "game\res"

where python >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [VROMFS] Упаковка конфигурации DataBlock .blk, DaScript .das и daRg .nut в game\res\gamedata.vromfs.bin...
    python prog\tools\pack_vromfs.py
)

where cl.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [MSVC C++20] Компиляция клиента Dagor Engine 6.5, сервера и 3D-лаунчера...
    cl /nologo /std:c++20 /O2 /EHsc /utf-8 /D_USE_MATH_DEFINES /Iprog\dagorInclude /Iprog\engine_dagor_rt /Iprog engine_dx11\src\game_main.cpp /Fe:bin\win64\SteelFrontGame.exe /link /SUBSYSTEM:WINDOWS d3d11.lib dxgi.lib d3dcompiler.lib ws2_32.lib winmm.lib dwmapi.lib gdi32.lib user32.lib ole32.lib shell32.lib
    cl /nologo /std:c++20 /O2 /EHsc /utf-8 /D_USE_MATH_DEFINES /Iprog\dagorInclude /Iprog\engine_dagor_rt /Iprog engine_dx11\server\server_main.cpp /Fe:bin\win64\SteelFrontServer.exe /link /SUBSYSTEM:CONSOLE ws2_32.lib
    cl /nologo /std:c++20 /O2 /EHsc /utf-8 /D_USE_MATH_DEFINES /Iprog\dagorInclude /Iprog\engine_dagor_rt /Iprog engine_dx11\launcher\launcher_main.cpp /Fe:SteelFrontLauncher.exe /link /SUBSYSTEM:WINDOWS d3d11.lib dxgi.lib d3dcompiler.lib winhttp.lib dwmapi.lib msimg32.lib gdi32.lib user32.lib shell32.lib ole32.lib
    del *.obj >nul 2>nul
    echo [ГОТОВО] Все бинарные файлы x64 Dagor Engine 6.5 пересобраны!
    pause
    exit /b 0
)

where clang++ >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [Clang C++20] Компиляция клиента Dagor Engine 6.5, сервера и 3D-лаунчера...
    clang++ -std=c++20 -O2 -D_USE_MATH_DEFINES -Iprog/dagorInclude -Iprog/engine_dagor_rt -Iprog engine_dx11/src/game_main.cpp -o bin/win64/SteelFrontGame.exe -Wl,/subsystem:windows -ld3d11 -ldxgi -ld3dcompiler -lws2_32 -lwinmm -ldwmapi -lgdi32 -luser32 -lole32 -lshell32
    clang++ -std=c++20 -O2 -D_USE_MATH_DEFINES -Iprog/dagorInclude -Iprog/engine_dagor_rt -Iprog engine_dx11/server/server_main.cpp -o bin/win64/SteelFrontServer.exe -Wl,/subsystem:console -lws2_32
    clang++ -std=c++20 -O2 -D_USE_MATH_DEFINES -Iprog/dagorInclude -Iprog/engine_dagor_rt -Iprog engine_dx11/launcher/launcher_main.cpp -o SteelFrontLauncher.exe -Wl,/subsystem:windows -ld3d11 -ldxgi -ld3dcompiler -lwinhttp -ldwmapi -lmsimg32 -lgdi32 -luser32 -lshell32 -lole32
    echo [ГОТОВО] Все бинарные файлы x64 Dagor Engine 6.5 пересобраны!
    pause
    exit /b 0
)

echo [ИНФО] Готовые исполняемые файлы Dagor Engine 6.5 уже скомпилированы:
echo   - SteelFrontLauncher.exe
echo   - bin\win64\SteelFrontGame.exe
echo   - bin\win64\SteelFrontServer.exe
echo   - game\res\gamedata.vromfs.bin
pause
