@echo off
chcp 65001 >nul
title Steel Front: Online — Сборка и развёртывание Gaijin Dagor Engine 6.5
cd /d "%~dp0"

echo ============================================================================
echo   STEEL FRONT: ONLINE — GAIJIN DAGOR ENGINE 6.5 (3 РЕЖИМА РАЗВЁРТЫВАНИЯ)
echo ============================================================================
echo   [1] Пересобрать архив VROMFS (game\res\gamedata.vromfs.bin) из живых .blk
echo   [2] Скачать и запустить предсобранный Gaijin daNetGame с CDN Gaijin
echo   [3] Развернуть и собрать полный 200-ГБ SDK GaijinEntertainment/DagorEngine
echo ============================================================================
set /p CHOICE="Выберите режим (1 / 2 / 3): "

if "%CHOICE%"=="1" (
    python prog\tools\pack_vromfs.py
    pause
    exit /b 0
)
if "%CHOICE%"=="2" (
    powershell -ExecutionPolicy Bypass -File "%~dp0SETUP_FULL_DAGOR_ENGINE.ps1" -Mode dng-prebuilt
    exit /b 0
)
if "%CHOICE%"=="3" (
    powershell -ExecutionPolicy Bypass -File "%~dp0SETUP_FULL_DAGOR_ENGINE.ps1" -Mode full-sdk
    exit /b 0
)
