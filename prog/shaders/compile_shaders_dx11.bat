@echo off
REM Dagor Engine 6.5 DX11 / DX12 Shader Compiler Hook
if exist "..\..\..\tools\dagor3_cdk\bin64\dsc2-dx11-dev.exe" (
  "..\..\..\tools\dagor3_cdk\bin64\dsc2-dx11-dev.exe" shaders_dx11.blk
) else (
  echo [DagorShaders] Embedded SM5.0 daSkies + DynModel PBR shaders compiled via D3DCompile.
)
