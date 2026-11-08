@echo off
chcp 65001 >nul
title Сборка Steel Front: Online (C++17 DirectX 11 Windows x64)
cd /d "%~dp0\.."
if not exist "bin\win64" mkdir "bin\win64"

where cl.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [MSVC] Компиляция клиента DirectX 11, сервера и лаунчера через Visual Studio C++17...
    cl /nologo /std:c++17 /O2 /EHsc /utf-8 engine_dx11\src\game_main.cpp /Fe:bin\win64\SteelFrontGame.exe /link /SUBSYSTEM:WINDOWS d3d11.lib dxgi.lib d3dcompiler.lib ws2_32.lib winmm.lib dwmapi.lib gdi32.lib user32.lib ole32.lib shell32.lib
    cl /nologo /std:c++17 /O2 /EHsc /utf-8 engine_dx11\server\server_main.cpp /Fe:bin\win64\SteelFrontServer.exe /link /SUBSYSTEM:CONSOLE ws2_32.lib
    cl /nologo /std:c++17 /O2 /EHsc /utf-8 engine_dx11\launcher\launcher_main.cpp /Fe:SteelFrontLauncher.exe /link /SUBSYSTEM:WINDOWS winhttp.lib dwmapi.lib msimg32.lib gdi32.lib user32.lib shell32.lib ole32.lib
    del *.obj >nul 2>nul
    echo [ГОТОВО] Все бинарные файлы x64 пересобраны!
    pause
    exit /b 0
)

where clang++ >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [Clang] Компиляция клиента DirectX 11, сервера и лаунчера...
    clang++ -std=c++17 -O2 engine_dx11/src/game_main.cpp -o bin/win64/SteelFrontGame.exe -Wl,/subsystem:windows -ld3d11 -ldxgi -ld3dcompiler -lws2_32 -lwinmm -ldwmapi -lgdi32 -luser32 -lole32 -lshell32
    clang++ -std=c++17 -O2 engine_dx11/server/server_main.cpp -o bin/win64/SteelFrontServer.exe -Wl,/subsystem:console -lws2_32
    clang++ -std=c++17 -O2 engine_dx11/launcher/launcher_main.cpp -o SteelFrontLauncher.exe -Wl,/subsystem:windows -lwinhttp -ldwmapi -lmsimg32 -lgdi32 -luser32 -lshell32 -lole32
    echo [ГОТОВО] Все бинарные файлы x64 пересобраны!
    pause
    exit /b 0
)

echo [ИНФО] Готовые исполняемые файлы уже скомпилированы и лежат в:
echo   - SteelFrontLauncher.exe
echo   - bin\win64\SteelFrontGame.exe
echo   - bin\win64\SteelFrontServer.exe
echo Для пересборки исходников C++17 запустите этот скрипт из Developer Command Prompt for VS 2022.
pause
