@echo off
REM Dagor Engine 6.5 Asset Builder (daBuild wrapper for Steel Front: Online)
if defined GDEVTOOL (
  if exist "..\..\tools\dagor3_cdk\bin64\dabuild-dev.exe" (
    "..\..\tools\dagor3_cdk\bin64\dabuild-dev.exe" ..\application.blk %*
    exit /b %ERRORLEVEL%
  )
)
python ..\prog\tools\pack_vromfs.py
