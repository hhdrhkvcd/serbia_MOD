$ErrorActionPreference = "Stop"

Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host " STEEL FRONT: ONLINE — GAIJIN DAGOR ENGINE SDK INSTALLER" -ForegroundColor Green
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Внимание: официальный инструментарий Gaijin Dagor Engine (MSVC, LLVM, DX12 SDK, jam, daBuild)" -ForegroundColor Yellow
Write-Host "требует Windows 10/11 x64, Python 3, Git, 7-Zip и около 150-200 ГБ свободного места." -ForegroundColor Yellow
Write-Host "(Если хотите играть прямо сейчас без скачивания 200 ГБ — запустите START_ONLINE_GAME.bat в корне!)" -ForegroundColor Green
Write-Host ""

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$WorkRoot = Join-Path $ScriptDir "DagorWorkspace"
$EngineDir = Join-Path $WorkRoot "DagorEngine"
$DevtoolsDir = Join-Path $WorkRoot "devtools"
$ModSrc = Join-Path $ScriptDir "steelFrontOnline"

New-Item -ItemType Directory -Force -Path $WorkRoot | Out-Null

if (-not (Test-Path (Join-Path $EngineDir ".git"))) {
    Write-Host "==> 1/4 Клонирование официального репозитория GaijinEntertainment/DagorEngine..." -ForegroundColor Cyan
    git clone --depth 1 https://github.com/GaijinEntertainment/DagorEngine.git $EngineDir
} else {
    Write-Host "==> 1/4 Репозиторий DagorEngine уже клонирован: $EngineDir" -ForegroundColor Green
}

Write-Host "==> 2/4 Копирование проекта steelFrontOnline внутрь дерева DagorEngine..." -ForegroundColor Cyan
$TargetProj = Join-Path $EngineDir "steelFrontOnline"
if (Test-Path $TargetProj) { Remove-Item -Recurse -Force $TargetProj }
Copy-Item -Recurse -Force $ModSrc $TargetProj

Write-Host "==> 3/4 Установка и настройка компиляторов Dagor devtools (make_devtools.py)..." -ForegroundColor Cyan
Push-Location $EngineDir
python make_devtools.py $DevtoolsDir

$env:GDEVTOOL = $DevtoolsDir
$env:PATH = "$DevtoolsDir;$env:PATH"

Write-Host "==> 4/4 Сборка клиента и выделенного онлайн-сервера Steel Front: Online (jam + daScript AOT)..." -ForegroundColor Cyan
Push-Location (Join-Path $TargetProj "prog")
python build.py code vromfs
Pop-Location
Pop-Location

Write-Host ""
Write-Host "ГОТОВО! Исполняемые файлы Dagor Engine собраны в: $TargetProj\game" -ForegroundColor Green
