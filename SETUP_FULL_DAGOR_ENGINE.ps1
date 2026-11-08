param(
    [ValidateSet("dng-prebuilt", "full-sdk")]
    [string]$Mode = "dng-prebuilt"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Set-Location -Path $PSScriptRoot

$DngViewerUrl = "https://dagorenginedata.cdn.gaijin.net/head-2026.11.07/dngSceneViewer.7z"
$ToolsBaseUrl = "https://dagorenginedata.cdn.gaijin.net/head-2026.11.07/tools-base.7z"
$ToolsPreUrl  = "https://dagorenginedata.cdn.gaijin.net/rel-0ebc89d5d795f3f96324843abf72c5ca7b8555cf/tools-prebuilt-windows-x86_64.7z"

function Find-7Zip {
    $candidates = @(
        "7z.exe",
        "$env:ProgramFiles\7-Zip\7z.exe",
        "${env:ProgramFiles(x86)}\7-Zip\7z.exe"
    )
    foreach ($c in $candidates) {
        if (Get-Command $c -ErrorAction SilentlyContinue) { return $c }
        if (Test-Path $c) { return $c }
    }
    return $null
}

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "  STEEL FRONT: ONLINE — GAIJIN DAGOR ENGINE 6.5 DEPLOYMENT ($Mode)" -ForegroundColor Yellow
Write-Host "======================================================================" -ForegroundColor Cyan

$sevenZip = Find-7Zip
if (-not $sevenZip) {
    Write-Host "[!] Для распаковки архивов .7z с CDN Gaijin требуется 7-Zip (https://www.7-zip.org/)." -ForegroundColor Red
    Write-Host "    Пытаюсь установить 7-Zip автоматически через winget..." -ForegroundColor Yellow
    winget install --id 7zip.7zip -e --accept-source-agreements --accept-package-agreements
    $sevenZip = Find-7Zip
    if (-not $sevenZip) {
        throw "7-Zip не найден. Установите 7-Zip и запустите режим повторно."
    }
}

if ($Mode -eq "dng-prebuilt") {
    $outDir = Join-Path $PSScriptRoot "dagor_prebuilt"
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $archFile = Join-Path $outDir "dngSceneViewer.7z"

    if (-not (Test-Path $archFile)) {
        Write-Host "[1/3] Скачивание официального предсобранного пакета Gaijin daNetGame (dngSceneViewer.7z)..." -ForegroundColor Green
        curl.exe -L $DngViewerUrl -o $archFile
    }

    Write-Host "[2/3] Распаковка бинарников Dagor Engine (windows-x86_64)..." -ForegroundColor Green
    & $sevenZip x $archFile "-o$outDir" -y | Out-Null

    Write-Host "[3/3] Монтирование модуля Steel Front: Online (application.blk, .blk бронетехники, DaScript ES)..." -ForegroundColor Green
    $modTarget = Join-Path $outDir "steelFrontOnline"
    New-Item -ItemType Directory -Force -Path $modTarget | Out-Null
    Copy-Item -Force "application.blk" (Join-Path $modTarget "application.blk")
    Copy-Item -Recurse -Force "prog" (Join-Path $modTarget "prog")

    $exeFound = Get-ChildItem -Path $outDir -Recurse -Filter "*win64*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $exeFound) {
        $exeFound = Get-ChildItem -Path $outDir -Recurse -Filter "*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    }

    if ($exeFound) {
        Write-Host "[ГОТОВО] Запуск нативного бинарника Gaijin Dagor Engine: $($exeFound.FullName)" -ForegroundColor Cyan
        Start-Process -FilePath $exeFound.FullName -WorkingDirectory $exeFound.DirectoryName
    } else {
        Write-Host "[ГОТОВО] Пакет распакован в $outDir" -ForegroundColor Cyan
        Start-Process explorer.exe $outDir
    }
    exit 0
}

# Mode == full-sdk: полный 200 ГБ SDK GaijinEntertainment/DagorEngine + devtools + jam
$parentDir = Split-Path -Parent $PSScriptRoot
$dagorRoot = Join-Path $parentDir "DagorEngine"
$devTools  = Join-Path $parentDir "devtools"

if (-not (Test-Path (Join-Path $dagorRoot ".git"))) {
    Write-Host "[1/5] Клонирование официального репозитория GaijinEntertainment/DagorEngine..." -ForegroundColor Green
    git clone --depth 1 https://github.com/GaijinEntertainment/DagorEngine.git $dagorRoot
}

Write-Host "[2/5] Настройка компиляторов и SDK Dagor (make_devtools.py -> $devTools)..." -ForegroundColor Green
Push-Location $dagorRoot
python make_devtools.py $devTools

$tbArchive = Join-Path $dagorRoot "tools-base.7z"
if (-not (Test-Path $tbArchive)) {
    Write-Host "[3/5] Загрузка tools-base.7z с CDN Gaijin..." -ForegroundColor Green
    curl.exe -L $ToolsBaseUrl -o $tbArchive
    & $sevenZip x $tbArchive "-o$dagorRoot" -y | Out-Null
}

$tpArchive = Join-Path $dagorRoot "tools-prebuilt-windows-x86_64.7z"
if (-not (Test-Path $tpArchive)) {
    Write-Host "[4/5] Загрузка предсобранного инструментария Dagor CDK (tools-prebuilt-windows-x86_64.7z)..." -ForegroundColor Green
    curl.exe -L $ToolsPreUrl -o $tpArchive
    & $sevenZip x $tpArchive "-o$dagorRoot" -y | Out-Null
}

$sfDest = Join-Path $dagorRoot "steelFrontOnline"
New-Item -ItemType Directory -Force -Path $sfDest | Out-Null
Copy-Item -Force (Join-Path $PSScriptRoot "application.blk") (Join-Path $sfDest "application.blk")
Copy-Item -Recurse -Force (Join-Path $PSScriptRoot "prog") (Join-Path $sfDest "prog")
Copy-Item -Recurse -Force (Join-Path $PSScriptRoot "develop") (Join-Path $sfDest "develop")

Write-Host "[5/5] Компиляция Steel Front: Online через jam + daScript AOT + vromfsPacker..." -ForegroundColor Green
Set-Location (Join-Path $sfDest "prog")
$env:GDEVTOOL = $devTools
$env:PATH = "$devTools\jam;$env:PATH"
python build.py code vromfs shaders
Pop-Location

Write-Host "[ГОТОВО] Полная сборка Gaijin Dagor Engine завершена: $sfDest\game" -ForegroundColor Cyan
Pause
