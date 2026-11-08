$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$base = if (Test-Path 'D:\') { 'D:\DagorDev' } else { "$env:SystemDrive\DagorDev" }
$engine = Join-Path $base 'DagorEngine'
$pack = Join-Path $base 'SteelFront_Pack'
$project = Join-Path $base 'SteelFront'
$downloads = Join-Path $base 'Downloads'
$log = Join-Path $base 'SteelFront-install.log'
$tag = 'dagor_2026_08_01'
$release = 'https://dagorenginedata.cdn.gaijin.net/rel-0ebc89d5d795f3f96324843abf72c5ca7b8555cf'
$devsrcUrl = 'https://dagorenginedata.cdn.gaijin.net/head-2026.08.01/outerSpace-devsrc.7z'
$pageUrl = 'https://raw.githubusercontent.com/hhdrhkvcd/serbia_MOD/steel-front-download/index.html'
$zipHash = '3e439055f5fdc25fe2392e12e9f62f3da270abcdcf08c0ec7be5e78b5a3a20d1'

New-Item -ItemType Directory -Force -Path $base,$downloads | Out-Null
try { Start-Transcript -Path $log -Append -Force | Out-Null } catch {}

function Refresh-Path {
  $env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
  @("$env:ProgramFiles\Git\cmd","$env:ProgramFiles\Git\bin","$env:ProgramFiles\7-Zip","$env:WINDIR",(Join-Path $env:LOCALAPPDATA 'Programs\Python\Launcher')) | ForEach-Object {
    if ($_ -and (Test-Path $_)) { $env:Path = "$_;$env:Path" }
  }
}
function FindExe([string]$name,[string[]]$paths=@()) {
  $c=Get-Command $name -ErrorAction SilentlyContinue
  if($c){return $c.Source}
  foreach($p in $paths){if($p -and (Test-Path -LiteralPath $p -PathType Leaf)){return $p}}
  return $null
}
function EnsurePkg([string]$id,[string]$name,[string]$exe,[string[]]$paths=@()) {
  Refresh-Path
  if(FindExe $exe $paths){Write-Host "$name уже установлен." -ForegroundColor Green;return}
  $wg=FindExe 'winget.exe' @("$env:LOCALAPPDATA\Microsoft\WindowsApps\winget.exe")
  if(-not $wg){throw "Не найден $name и нет winget. Установи его вручную: Git https://git-scm.com/download/win ; 7-Zip https://www.7-zip.org/ ; Python https://www.python.org/downloads/windows/"}
  Write-Host "Установка $name..." -ForegroundColor Cyan
  & $wg install --id $id --exact --silent --accept-package-agreements --accept-source-agreements
  Refresh-Path
  if(-not (FindExe $exe $paths)){throw "Не удалось установить $name через winget."}
}
function Download([string]$url,[string]$file,[long]$minimum) {
  if((Test-Path $file) -and (Get-Item $file).Length -ge $minimum){Write-Host "Уже загружено: $file";return}
  $curl=FindExe 'curl.exe' @("$env:WINDIR\System32\curl.exe")
  if(-not $curl){throw 'В Windows не найден curl.exe.'}
  Write-Host "Скачиваю $([IO.Path]::GetFileName($file))..." -ForegroundColor Cyan
  & $curl -fL --retry 5 --retry-delay 3 --connect-timeout 30 -C - -o $file $url
  if($LASTEXITCODE -ne 0){throw "Ошибка curl $LASTEXITCODE при скачивании $url"}
  if((Get-Item $file).Length -lt $minimum){throw "Файл загружен не полностью: $file. Запусти BAT снова, загрузка продолжится."}
}

try {
  Write-Host 'STEEL FRONT ONLINE — автоустановка Dagor Engine' -ForegroundColor Cyan
  $drv=[IO.Path]::GetPathRoot($base).Substring(0,1)
  $free=(Get-PSDrive -Name $drv).Free
  Write-Host ("Свободно на диске {0}: {1:N1} GB" -f $drv,($free/1GB))
  if($free -lt 25GB){throw 'Нужно минимум 25 GB свободного места.'}

  EnsurePkg 'Git.Git' 'Git' 'git.exe' @("$env:ProgramFiles\Git\cmd\git.exe")
  EnsurePkg '7zip.7zip' '7-Zip' '7z.exe' @("$env:ProgramFiles\7-Zip\7z.exe","$( [Environment]::GetFolderPath('ProgramFilesX86') )\7-Zip\7z.exe")
  EnsurePkg 'Python.Python.3.12' 'Python 3.12' 'py.exe' @("$env:WINDIR\py.exe",(Join-Path $env:LOCALAPPDATA 'Programs\Python\Launcher\py.exe'))

  $git=FindExe 'git.exe' @("$env:ProgramFiles\Git\cmd\git.exe")
  $seven=FindExe '7z.exe' @("$env:ProgramFiles\7-Zip\7z.exe","$( [Environment]::GetFolderPath('ProgramFilesX86') )\7-Zip\7z.exe")
  if(-not $git -or -not $seven -or -not (FindExe 'py.exe' @("$env:WINDIR\py.exe"))){throw 'Не нашёл Git, 7-Zip или Python после установки.'}

  $zip=Join-Path $downloads 'SteelFront_Dagor_Online_v0.1.zip'
  if(-not (Test-Path $zip) -or (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $zipHash){
    $htmlFile=Join-Path $downloads 'download-page.html'
    Download $pageUrl $htmlFile 300000
    $html=[IO.File]::ReadAllText($htmlFile)
    $m=[regex]::Match($html,'const archiveBase64 = "([A-Za-z0-9+/=]+)";')
    if(-not $m.Success){throw 'Не удалось извлечь ZIP с GitHub download page.'}
    [IO.File]::WriteAllBytes($zip,[Convert]::FromBase64String($m.Groups[1].Value))
    if((Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $zipHash){Remove-Item $zip -Force;throw 'Контрольная сумма ZIP не совпала.'}
  }
  Write-Host 'Архив Steel Front проверен.' -ForegroundColor Green

  if(-not (Test-Path (Join-Path $engine 'build_all.py'))){
    if(Test-Path $engine){throw "Папка $engine существует, но это не Dagor Engine. Переименуй её и повтори запуск; ничего не удалено."}
    Write-Host "Клонирование Dagor Engine ($tag)..." -ForegroundColor Cyan
    & $git -c core.longpaths=true clone --depth 1 --single-branch --branch $tag 'https://github.com/GaijinEntertainment/DagorEngine.git' $engine
    if($LASTEXITCODE -ne 0){throw "git clone завершился с кодом $LASTEXITCODE"}
  }

  $client=Join-Path $engine 'outerSpace\game\windows-x86_64\outer_space-dev.exe'
  $ded=Join-Path $engine 'outerSpace\game\windows-x86_64\outer_space-ded-dev.exe'
  if(-not (Test-Path $client) -or -not (Test-Path $ded)){
    $pre=Join-Path $downloads 'outerSpace-prebuilt-fullsrc-windows.7z'
    Download "$release/outerSpace-prebuilt-fullsrc-windows.7z" $pre 600MB
    Write-Host 'Распаковка готового Windows-клиента и серверных файлов...' -ForegroundColor Cyan
    & $seven x $pre "-o$engine" -y -aos
    if($LASTEXITCODE -gt 1){throw "7-Zip вернул код $LASTEXITCODE"}
  }
  if(-not (Test-Path $client) -or -not (Test-Path $ded)){throw 'После распаковки не найдены Outer Space client и dedicated server.'}

  $asset=Join-Path $engine 'outerSpace\develop\assets\content\outer_space\levels\sample_gaija_planet.blk'
  if(-not (Test-Path $asset)){
    $dev=Join-Path $downloads 'outerSpace-devsrc.7z'
    Download $devsrcUrl $dev 100MB
    & $seven x $dev "-o$engine" -y -aos
    if($LASTEXITCODE -gt 1){throw "7-Zip вернул код $LASTEXITCODE при распаковке devsrc."}
  }

  if(Test-Path $pack){Remove-Item $pack -Recurse -Force}
  New-Item -ItemType Directory -Force -Path $pack | Out-Null
  Expand-Archive -LiteralPath $zip -DestinationPath $pack -Force
  $pkg=Join-Path $pack 'SteelFront_Dagor_Online'
  $installer=Join-Path $pkg 'Install-SteelFront-Dagor.ps1'
  if(-not (Test-Path $installer)){throw "В ZIP не найден $installer"}

  Write-Host 'Устанавливаю файлы Steel Front...' -ForegroundColor Cyan
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -EngineRoot $engine -ProjectRoot $project
  if($LASTEXITCODE -ne 0){throw "Проектный установщик завершился с кодом $LASTEXITCODE"}

  $run=Join-Path $project 'Run-Local-Multiplayer.ps1'
  if(-not (Test-Path $run)){throw "Не найден $run"}
  Write-Host 'Запускаю локальный сервер и клиент...' -ForegroundColor Green
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $run -EngineRoot $engine
  if($LASTEXITCODE -ne 0){throw "Сценарий запуска завершился с кодом $LASTEXITCODE"}
  Write-Host 'Запуск завершён. Оставь открытым окно master server.' -ForegroundColor Green
  Write-Host "Лог установки: $log"
}
catch {
  Write-Host ("ОШИБКА: " + $_.Exception.Message) -ForegroundColor Red
  Write-Host "Подробный лог: $log" -ForegroundColor Yellow
  try { Stop-Transcript | Out-Null } catch {}
  exit 1
}
finally { try { Stop-Transcript | Out-Null } catch {} }
exit 0
