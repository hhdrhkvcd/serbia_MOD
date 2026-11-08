#!/usr/bin/env python3
"""
Скрипт сборки клиента и выделенного онлайн-сервера Steel Front: Online
в составе открытого Gaijin Dagor Engine (https://github.com/GaijinEntertainment/DagorEngine).
"""
import sys
import os

sys.path.append('../..')
from build_all import (
  run,
  run_per_platform,
  VROMFS_PACKER_EXE,
  DABUILD_CMD,
  BUILD_COMPONENTS,
  DAGOR_HOST,
  DAGOR_HOST_ARCH,
  JAM_BUILD_TARGET_ARCH_OPTIONS,
)
sys.path.pop()

PROJ_JAM_OPTIONS = list(JAM_BUILD_TARGET_ARCH_OPTIONS)
if DAGOR_HOST == 'windows' and not PROJ_JAM_OPTIONS:
  PROJ_JAM_OPTIONS += [f'-sPlatformArch={DAGOR_HOST_ARCH}']

if 'code' in BUILD_COMPONENTS:
  # 1. Сборка AOT-компилятора DaScript для боевых ECS-систем танков
  run([
    'jam',
    '-sRoot=../..',
    '-sProjectLocation=steelFrontOnline/prog',
    '-sTarget=steel_front-aot',
    '-sOutDir=../tools/das-aot',
    '-f', '../../prog/daNetGame-das-aot/jamfile',
  ] + PROJ_JAM_OPTIONS)

  # 2. Сборка клиента игры (DX12 / Vulkan PBR)
  run(['jam', '-sNeedDasAotCompile=yes'] + PROJ_JAM_OPTIONS)

  # 3. Сборка выделенного онлайн-сервера (-sDedicated=yes) как в War Thunder / Enlisted
  run(['jam', '-sNeedDasAotCompile=yes', '-sDedicated=yes'] + PROJ_JAM_OPTIONS)

if 'vromfs' in BUILD_COMPONENTS:
  run([VROMFS_PACKER_EXE, 'mk.vromfs.blk', '-quiet', '-addpath:.'], cwd='.')

if 'assets' in BUILD_COMPONENTS:
  if os.path.isdir('../develop'):
    run(DABUILD_CMD + ['../application.blk'], cwd='../develop')
