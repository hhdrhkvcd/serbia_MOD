@echo off
chcp 65001 >nul
title Steel Front: Online — Gaijin Dagor Engine Builder
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0SETUP_DAGOR.ps1"
pause
