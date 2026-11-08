@echo off
chcp 65001 >nul
title Steel Front: Online — Сборка и загрузка билда в SteamPipe
echo ============================================================================
echo   STEEL FRONT: ONLINE — STEAMPIPE UPLOADER (Windows x64 DirectX 11)
echo ============================================================================
echo   1. При наличии собственного Steam AppID укажите его в app_build.vdf.
echo   2. При наличии steam_api64.dll из вашего Steamworks SDK положите её
echo      рядом с bin\win64\SteelFrontGame.exe — игра подхватит её автоматически!
echo   3. Для мгновенной проверки оверлея Steam (Shift+Tab в DirectX 11) просто
echo      добавьте SteelFrontLauncher.exe или bin\win64\SteelFrontGame.exe
echo      в клиент Steam: «Игры -> Добавить стороннюю игру в мою библиотеку».
echo ============================================================================
echo.
where steamcmd >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ИНФО] Утилита steamcmd.exe не найдена в PATH.
    echo Скачайте SteamCMD с https://developer.valvesoftware.com/wiki/SteamCMD
    pause
    exit /b 0
)
set /p STEAM_LOGIN="Введите логин партнёра Steamworks: "
steamcmd +login %STEAM_LOGIN% +run_app_build "%~dp0app_build.vdf" +quit
pause
