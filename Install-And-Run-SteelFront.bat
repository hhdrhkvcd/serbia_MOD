@echo off
setlocal EnableExtensions
title STEEL FRONT ONLINE - AUTO INSTALL
chcp 65001 >nul

set "BOOT_PS1=%TEMP%\SteelFrontBootstrap_%RANDOM%.ps1"
set "BAT_FILE=%~f0"

echo =====================================================
echo       STEEL FRONT ONLINE - AUTOMATIC INSTALL
echo =====================================================
echo.
echo This BAT installs required tools, downloads the public Dagor Engine
echo source, official Windows tools and Outer Space client/server,
echo then prepares and starts the non-combat driving prototype.
echo.
echo Initial download is about 1 GB or more. Keep this window open.
echo At least 30 GB of free space is required on D:.
echo.

where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: Windows PowerShell was not found.
    pause
    exit /b 1
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $f='%BAT_FILE%'; $p='%BOOT_PS1%'; $c=[IO.File]::ReadAllText($f); $m='### POWERSHELL SCRIPT BELOW ###'; $i=$c.IndexOf($m); if($i -lt 0){throw 'Embedded setup script marker missing.'}; $body=$c.Substring($i+$m.Length); [IO.File::WriteAllText($p,$body,[Text.UTF8Encoding]::new($false)); & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File $p; exit $LASTEXITCODE"
set "RESULT=%ERRORLEVEL%"

echo.
if not "%RESULT%"=="0" (
    echo Setup stopped with error code %RESULT%.
    echo Send the full error text and %LOCALAPPDATA%\SteelFront\install-and-run.log to ChatGPT.
) else (
    echo Setup process finished.
l)
echo.
pause
exit /b %RESULT%

### POWERSHELL SCRIPT BELOW ###
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$Root = 'D:\DagorDev'
$EngineRoot = Join-Path $Root 'DagorEngine_SteelFront_2026_08_01'
$ProjectPack = Join-Path $Root 'SteelFront_Pack'
$ProjectRoot = Join-Path $Root 'SteelFront'
$Downloads = Join-Path $Root 'Downloads'
$LogDir = Join-Path $env:LOCALAPPDATA 'SteelFront'
$LogPath = Join-Path $LogDir 'install-and-run.log'
$TranscriptStarted = $false

function Refresh-Path {
    $machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = @($machinePath, $userPath) -join ';'
}

function Get-SevenZip {
    $candidates = @(
        "$env:ProgramFiles\7-Zip\7z.exe",
        "$env:LOCALAPPDATA\Programs\7-Zip\7z.exe",
        "$Root\devtools\7-Zip\7z.exe"
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $candidate }
    }
    $cmd = Get-Command 7z.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Install-WingetPackage([string]$Id) {
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw "Не найден winget. Установи App Installer из Microsoft Store, затем запусти этот BAT повторно. Не удалось автоматически установить пакет: $Id"
    }
    Write-Host "Устанавливаю зависимость $Id через winget..." -ForegroundColor Cyan
    & $winget.Source install --id $Id --exact --silent --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) { throw "Установка $Id завершилась с кодом $LASTEXITCODE." }
    Refresh-Path
}

function Find-PythonLauncher {
    Refresh-Path
    $cmd = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @(
        "$env:LOCALAPPDATA\Programs\Python\Launcher\py.exe",
        "$env:WINDIR\py.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    return $null
}

function Download-File([string]$Url, [string]$Destination, [long]$MinimumBytes) {
    if ((Test-Path -LiteralPath $Destination -PathType Leaf) -and (Get-Item -LiteralPath $Destination).Length -ge $MinimumBytes) {
        Write-Host "Уже скачано: $Destination" -ForegroundColor DarkYellow
        return
    }
    if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Force }
    Write-Host "Скачиваю: $([IO.Path]::GetFileName($Destination))" -ForegroundColor Cyan
    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
    if ($curl) {
        & $curl.Source -fL --retry 5 --retry-delay 3 --connect-timeout 30 $Url -o $Destination
        if ($LASTEXITCODE -ne 0) { throw "curl не смог скачать файл (код $LASTEXITCODE): $Url" }
    } else {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing
    }
    if (-not (Test-Path -LiteralPath $Destination) -or (Get-Item -LiteralPath $Destination).Length -lt $MinimumBytes) {
        throw "Файл скачан не полностью или CDN вернул ошибку: $Destination"
    }
}

function Expand-7Zip([string]$SevenZip, [string]$Archive, [string]$Destination) {
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Write-Host "Распаковываю $([IO.Path]::GetFileName($Archive)) ..." -ForegroundColor Cyan
    & $SevenZip x $Archive "-o$Destination" -aos -y
    if ($LASTEXITCODE -ne 0) { throw "7-Zip завершился с кодом $LASTEXITCODE при распаковке $Archive" }
}

try {
    New-Item -ItemType Directory -Force -Path $Root, $Downloads, $LogDir | Out-Null
    Start-Transcript -Path $LogPath -Append | Out-Null
    $TranscriptStarted = $true

    Write-Host ''
    Write-Host '============================================' -ForegroundColor Cyan
    Write-Host '   STEEL FRONT ONLINE - AUTO INSTALLER' -ForegroundColor Cyan
    Write-Host '   Dagor Engine / Windows x64' -ForegroundColor Cyan
    Write-Host '============================================' -ForegroundColor Cyan
    Write-Host 'Это длительная первоначальная установка: будут загружены исходники Dagor, инструменты и prebuilt-клиент Outer Space.' -ForegroundColor Yellow

    if (-not [Environment]::Is64BitOperatingSystem) { throw 'Нужна 64-разрядная Windows 10/11.' }
    if (-not (Test-Path -LiteralPath 'D:\')) { throw 'Диск D: не найден. В этом BAT путь установки задан как D:\DagorDev.' }
    $drive = Get-PSDrive -Name 'D' -ErrorAction Stop
    $freeGB = [math]::Round($drive.Free / 1GB, 1)
    Write-Host "Свободно на D: $freeGB GB"
    if ($drive.Free -lt 30GB) { throw "Нужно минимум 30 GB свободного места на D:. Сейчас: $freeGB GB. Полный SDK в целом требует существенно больше." }

    # Required extraction tool.
    $sevenZip = Get-SevenZip
    if (-not $sevenZip) {
        Install-WingetPackage '7zip.7zip'
        $sevenZip = Get-SevenZip
    }
    if (-not $sevenZip) { throw '7-Zip не найден после установки. Перезагрузи Windows и запусти BAT ещё раз.' }

    # Python launcher is used by the official Dagor resource build and the sample room server.
    $python = Find-PythonLauncher
    if (-not $python) {
        Install-WingetPackage 'Python.Pyton.3.13'
        $python = Find-PythonLauncher
    }
    if (-not $python) { throw 'Не найден py.exe / после установки Python. Перезагрузи Windows и сстановки.' }
    & $python -3 --version
    if ($LASTEXITCODE -ne 0) { throw 'Python Launcher py.exe зория, доблого запускается.' }

    # Pin source and binary archives to the same published Dagor build to avoid version skew.
    $tag = 'dagor_2026_08_01'
    $cdn = 'https://dagorenginedata.cdn.gaijin.net/rel-0ebc89d5d795f3f96324843abf72c5ca7b8555cf'
    $sourceZip = Join-Path $Downloads 'DagorEngine-dagor_2026_08_01.zip'
    $toolsArchive = Join-Path $Downloads 'tools-prebuilt-windows-x86_64.7z'
    $runtimeArchive = Join-Path $Downloads 'outerSpace-prebuilt-fullsrc-windows.7z'

    # Download public engine source only if this dedicated folder is not already complete.
    if (-not (Test-Path -LiteralPath (Join-Path $EngineRoot 'build_all.py')) -or -not (Test-Path -LiteralPath (Join-Path $EngineRoot 'outerSpace\prog\build.py'))) {
        if (Test-Path -LiteralPath $EngineRoot) {
            $backup = "$EngineRoot.backup_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
            Write-Host "Неполная папка движка сохранена как $backup" -ForegroundColor Yellow
            Move-Item -LiteralPath $EngineRoot -Destination $backup
        }
        $sourceUrl = "https://codeload.github.com/GaijinEntertainment/DagorEngine/zip/refs/tags/$tag"
        Download-File $sourceUrl $sourceZip 10000000
        $sourceExtract = Join-Path $Downloads 'DagorEngine_Source_Extract'
        if (Test-Path -LiteralPath $sourceExtract) { Remove-Item -LiteralPath $sourceExtract -Recurse -Force }
        Expand-Archive -LiteralPath $sourceZip -DestinationPath $sourceExtract -Force
        $sourceDir = Get-ChildItem -LiteralPath $sourceExtract -Directory | Select-Object -First 1
        if (-not $sourceDir -or -not (Test-Path -LiteralPath (Join-Path $sourceDir.FullName 'build_all.py'))) {
            throw 'В архиве GitHub не найдена корневая папка исходников Dagor Engine.'
        }
        Move-Item -LiteralPath $sourceDir.FullName -Destination $EngineRoot
    }

    # Download official Windows utilities and prebuilt Outer Space client/server.
    Download-File "$cdn/tools-prebuilt-windows-x86_64.7z" $toolsArchive 80000000
    Download-File "$cdn/outerSpace-prebuilt-fullsrc-windows.7z" $runtimeArchive 700000000
    Expand-7Zip $sevenZip $toolsArchive $EngineRoot
    Expand-7Zip $sevenZip $runtimeArchive $EngineRoot

    $clientExe = Join-Path $EngineRoot 'outerSpace\game\windows-x86_64\outer_space-dev.exe'
    $dedicatedExe = Join-Path $EngineRoot 'outerSpace\game\windows-x86_64\outer_space-ded-dev.exe'
    if (-not (Test-Path -LiteralPath $clientExe)) { throw "После распаковки не найден клиент: $clientExe" }
    if (-not (Test-Path -LiteralPath $dedicatedExe)) { throw "После распаковки не найден dedicated server: $dedicatedExe" }

    # Download project ZIP from the user's GitHub-hosted page (the ZIP is embedded in its HTML).
    $pageUrl = 'https://raw.githubusercontent.com/hhdrhkvcd/serbia_MOD/steel-front-download/SteelFront_Download/index.html'
    $pageFile = Join-Path $Downloads 'SteelFront_DownloadPage.html'
    $projectZip = Join-Path $Downloads 'SteelFront_Dagor_Online_v0.1.zip'
    Download-File $pageUrl $pageFile 10000
    $html = [IO.File]::ReadAllText($pageFile)
    if ($html -notmatch 'const\s+archiveBase64\s*=\s*"([A-Za-z0-9+/=]+)"') {
        throw 'Не удалось найти встроенный ZIP в странице загрузки проекта. Проверь доступность GitHub-репозитория.'
    }
    $zipBytes = [Convert]::FromBase64String($Matches[1])
    [IO.File]::WriteAllBytes($projectZip, $zipBytes)
    if ($zipBytes.Length -lt 100000) { throw 'Скачанный архив Steel Front подозрительно мал.' }
    New-Item -ItemType Directory -Force -Path $ProjectPack | Out-Null
    Expand-Archive -LiteralPath $projectZip -DestinationPath $ProjectPack -Force

    $packRoot = Join-Path $ProjectPack 'SteelFront_Dagor_Online'
    $installer = Join-Path $packRoot 'Install-SteelFront-Dagor.ps1'
    if (-not (Test-Path -LiteralPath $installer)) { throw "В архиве проекта отсутствует установщик: $installer" }

    # Match the devsrc fallback URL to the same 2026/08/01 release, if that archive is needed.
    $installerText = [IO.File]::ReadAllText($installer)
    $installerText = $installerText.Replace('head-2026.11.07', 'head-2026.08.01')
    [IO.File]::WriteAllText($installer, $installerText, (New-Object System.Text.UTF8Encoding($false)))

    Write-Host ''
    Write-Host 'Устанавливаю Steel Front в Dagor...' -ForegroundColor Cyan
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -EngineRoot $EngineRoot -ProjectRoot $ProjectRoot
    if ($LASTEXITCODE -ne 0) { throw "Установщик Steel Front завершился с кодом $LASTEXITCODE." }

    $buildScript = Join-Path $ProjectRoot 'Build-SteelFront.ps1'
    Write-Host ''
    Write-Host 'Собираю игровые ресурсы Steel Front...' -ForegroundColor Cyan
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $buildScript -EngineRoot $EngineRoot
    if ($LASTEXITCODE -ne 0) { throw "Сборка ресурсов завершилась с кодом $LASTEXITCODE. Лог: $LogPath" }

    $runScript = Join-Path $ProjectRoot 'Run-Local-Multiplayer.ps1'
    Write-Host ''
    Write-Host 'Запускаю локальный сервер комнат и клиент...' -ForegroundColor Cyan
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runScript -EngineRoot $EngineRoot
    if ($LASTEXITCODE -ne 0) { throw "Не удалось запустить локальную игру. Лог: $LogPath" }

    Write-Host ''
    Write-Host 'Готово: клиент запущен. Окно сервера оставь открытым.' -ForegroundColor Green
    Write-Host 'В клиенте введи ник; адрес сервера комнат: http://127.0.0.1:5000' -ForegroundColor Green
    Write-Host 'Для тестовой комнаты укажи сцену: steel_front_training' -ForegroundColor Green
    Write-Host "Лог установки: $LogPath" -ForegroundColor DarkGray
} catch {
    Write-Host ''
    Write-Host 'УСТАНОВКА / ЗАПУСК НЕ ЗАВЕРШИЛИСЬ' -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host "Подробный лог: $LogPath" -ForegroundColor Yellow
    exit 1
} finally {
    if ($TranscriptStarted) { Stop-Transcript | Out-Null }
}
