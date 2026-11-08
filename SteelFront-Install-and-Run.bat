@echo off
setlocal EnableExtensions
chcp 65001 >nul
title STEEL FRONT ONLINE - Installer
color 0B
echo ============================================================
echo   STEEL FRONT ONLINE - AUTO INSTALL AND LAUNCH
echo ============================================================
echo.
echo This BAT downloads and installs Dagor Engine / Outer Space.
echo Internet and at least 25 GB free disk space are required.
echo It may install Git, 7-Zip and Python using winget.
echo Do not close this window while it is running.
echo.
set "SETUP_PS1=%TEMP%\SteelFront-Setup.ps1"
set "SETUP_URL=https://raw.githubusercontent.com/hhdrhkvcd/serbia_MOD/steel-front-download/SteelFront-Setup.ps1"
where powershell.exe >nul 2>&1
if errorlevel 1 (
  echo ERROR: Windows PowerShell was not found.
  pause
  exit /b 1
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; try { Invoke-WebRequest -Uri '%SETUP_URL%' -OutFile '%SETUP_PS1%' -UseBasicParsing -TimeoutSec 120; if ((Get-Item -LiteralPath '%SETUP_PS1%').Length -lt 10000) { throw 'Downloaded installer is too small.' }; & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File '%SETUP_PS1%'; exit $LASTEXITCODE } catch { Write-Host ('SETUP DOWNLOAD ERROR: ' + $_.Exception.Message) -ForegroundColor Red; exit 1 }"
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo Setup or launch failed. Exit code: %RC%
  echo Setup log: D:\DagorDev\SteelFront-install.log
) else (
  echo Setup script finished. Check the game and master-server windows.
)
echo.
pause
exit /b %RC%
