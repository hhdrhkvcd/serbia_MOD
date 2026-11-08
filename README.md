# Steel Front: Online — Gaijin Dagor Engine 6.5 (Windows 10/11 x64)

Нативный многопользовательский танковый онлайн-экшен для ПК (**Windows 10 / 11 x64**), полностью переведённый на архитектуру **Gaijin Dagor Engine 6.5** сразу в **3 рабочих вариантах** в одном репозитории!

В строю **7 мировых держав** (**Россия, Сербия, Китай, США, НАТО/Германия-Британия, Франция, Швеция**) и **23 детальные 3D-машины**, включая полную бронетанковую ветку Вооружённых сил Сербии (**BVP M-80AB1**, **M-84AS1**, **M-91 Vihor**, **M-84AS2 «Војвода»**).

---

## Быстрый старт на ПК (Windows 10 / 11 x64)

### Способ А — Полный архив с GitHub (работает мгновенно из коробки)
1. Скачайте ZIP-архив ветки `arena/2571b3b1-serbia-mod` и распакуйте в любую папку.
2. Запустите **`SteelFrontLauncher.exe`** (живой 3D DirectX 11 лаунчер с витриной танков и выбором 3 режимов Dagor Engine).
3. Нажмите **«ИГРАТЬ»** — откроется ангар `bin\win64\SteelFrontGame.exe` с подключённым `application.blk` и архивом `game\res\gamedata.vromfs.bin`.

### Способ Б — Скачать только один файл `SteelFrontLauncher.exe` (717 КБ)
1. Скачайте один файл **`SteelFrontLauncher.exe`** в пустую папку и запустите его.
2. Лаунчер автоматически скачает по HTTPS с GitHub все файлы ядра Dagor 6.5 (`application.blk`, `game\res\gamedata.vromfs.bin`, `SETUP_FULL_DAGOR_ENGINE.ps1`, `bin\win64\SteelFrontGame.exe` и `bin\win64\SteelFrontServer.exe`) и активирует кнопку **«ИГРАТЬ»**.

---

## Сразу 3 режима Gaijin Dagor Engine 6.5 (переключаются прямо в Лаунчере!)

### Режим 1 — Нативное ядро Gaijin Dagor Engine 6.5 (`Quirrel VM` + `Dagor Math` + `daSkies2` + `Perlin HeightMapLand` + `daRg`)
* **Настоящие скомпилированные C++ модули `GaijinEntertainment/DagorEngine`** внутри исполняемых файлов x64 (`SteelFrontGame.exe` 1.7 МБ, `SteelFrontLauncher.exe` 1.6 МБ, `SteelFrontServer.exe` 1.4 МБ):
  * **Виртуальная машина и компилятор Gaijin Quirrel 4 (`prog/1stPartyLibs/quirrel/quirrel`)**: 42 исходных C++ модуля ВМ/компилятора/анализатора Quirrel (`.nut`), исполняющие скрипты из `VROMFS` и живые выражения `sq <код>` в консоли разработчика `~`!
  * **Математическое ядро Dagor Engine (`prog/engine/math` + `vecmath` SSE4.1)**: `perlin.cpp` (`perlin_noise::fBm3` для генерации реалистичного рельефа полигона `HeightMapLand`), `rndSeed.cpp`, `mathAng.cpp`, `math3d.cpp`, `wooray3d.cpp`, `triangleTriangleIntersection.cpp`.
  * **Атмосфера `daSkies2` + `clouds2` + `daStars`**: физическое рассеяние Ми (Хеньи—Гринштейна), 4 динамических пресета погоды/времени суток (**Закат Golden Hour**, **Полдень**, **Гроза**, **Звёздная ночь** — клавиша **`F4`**), двухслойные объёмные облака с серебристой окантовкой и бегущими по земле тенями облаков.
  * **Подвеска `vehiclePhys` + интерфейс `daRg` + калькулятор брони**: наклон корпуса по нормали ландшафта, клевок торсионной подвески при разгоне/торможении/выстреле, графитово-янтарный интерфейс в стиле Gaijin (`daRg`), виджет телеметрии движка **`F2`**, каркасная сетка **`F3`** и живой расчёт приведённой брони под курсором мыши в режиме **«Анализ брони»**!
* **Живой CDK-моддинг `.blk` без перезапуска игры**: нажмите кнопку **`CDK .BLK (F5)`** в шапке ангара — откроется `prog\gameBase\gamedata\units\tanks_all_nations.blk`. Измените любые ТТХ, сохраните и нажмите **`F5`** (или введите `blk.reload` в консоли `~`)!

### Режим 2 — Официальный предсобранный пакет Gaijin `daNetGame` (CDN Prebuilt)
* Выберите в лаунчере **`2. GAIJIN daNetGame (CDN PREBUILT x64)`** и нажмите **«РАЗВЕРНУТЬ»** (или запустите `BUILD_DAGOR_PROJECT.bat` -> `[2]`).
* Скрипт `SETUP_FULL_DAGOR_ENGINE.ps1 -Mode dng-prebuilt` автоматически скачивает официальный бинарный архив `dngSceneViewer.7z` с CDN Gaijin (`dagorenginedata.cdn.gaijin.net`), распаковывает нативные `windows-x86_64` исполняемые модули `daNetGame` и монтирует модуль `steelFrontOnline` (`application.blk`, `.blk`, DaScript `.das`, daRg `.nut`).

### Режим 3 — Полный исходный SDK `GaijinEntertainment/DagorEngine` (Git + Devtools + Jam)
* Выберите в лаунчере **`3. ПОЛНЫЙ DAGOR SDK (GIT + JAM • 200 ГБ)`** и нажмите **«СОБРАТЬ SDK»** (или запустите `BUILD_DAGOR_PROJECT.bat` -> `[3]`).
* Скрипт `SETUP_FULL_DAGOR_ENGINE.ps1 -Mode full-sdk` клонирует официальный репозиторий `https://github.com/GaijinEntertainment/DagorEngine.git`, разворачивает компиляторы через `make_devtools.py`, скачивает `tools-base.7z` и `tools-prebuilt-windows-x86_64.7z` с CDN Gaijin и собирает проект через канонический пайплайн `prog/build.py` (`jam` + `daNetGame-das-aot` + `vromfsPacker-dev.exe` + `compile_shaders_dx11.bat` / `dx12`).

---

## Каноническая структура проекта Dagor Engine 6.5

```text
serbia_MOD/
├── application.blk                  # Главный манифест Dagor Engine 6.5 (mountPoints, vromfs, daSkies)
├── SteelFrontLauncher.exe           # 3D DirectX 11 лаунчер с переключателем всех 3 режимов Dagor
├── BUILD_DAGOR_PROJECT.bat          # Консольное меню сборки VROMFS / CDN daNetGame / Full SDK
├── SETUP_FULL_DAGOR_ENGINE.ps1      # Автоустановщик пакетов с CDN Gaijin и полного SDK DagorEngine
├── bin/win64/
│   ├── SteelFrontGame.exe           # Боевой клиент Win64 (Dagor 6.5 Core + VROMFS + .BLK + DX11 PBR)
│   └── SteelFrontServer.exe         # Авторитативный выделенный UDP-сервер (60 Гц + чтение .blk)
├── game/res/
│   └── gamedata.vromfs.bin          # Собранный виртуальный архив VRFs v6.5 (13 файлов .blk/.das/.nut)
├── prog/
│   ├── jamfile & build.py           # Канонические скрипты сборки Gaijin Jam & Python
│   ├── mk.vromfs.blk                # Правила упаковки VROMFS для vromfsPacker-dev.exe
│   ├── dagorInclude/                # Официальные заголовочные файлы Gaijin Dagor Engine 6.5 (BSD-3)
│   ├── engine_dagor_rt/             # Рантайм DataBlock (.blk), VROMFS mounter и менеджер мира daECS
│   ├── gameBase/gamedata/           # Живые конфиги .blk (23 танка, орудия, камуфляжи, 4 карты, шаблоны)
│   ├── scripts/                     # Боевая логика DaScript (.das) и интерфейс ангара/HUD Quirrel (.nut)
│   ├── shaders/                     # Скрипты компиляции шейдеров Dagor dshl (DX11 / DX12)
│   └── tools/pack_vromfs.py         # Автономный упаковщик архивов gamedata.vromfs.bin
├── develop/                         # Исходные ассеты daBuild (dabuild.cmd, gui/fonts/fontgenPC.blk)
├── engine_dx11/                     # Исходный код C++20 клиента, UDP-сервера, PBR-рендера и лаунчера
└── steam_sdk/                       # Конфигурация Steamworks SDK и скрипты публикации SteamPipe
```

---

## Управление в бою и ангаре

* **`W / S`** — движение вперёд / задний ход
* **`A / D`** — поворот корпуса влево / вправо (классическая неинвертированная схема)
* **`Мышь`** — наведение башни и орудия (без инверсии по X/Y)
* **`ЛКМ`** — выстрел главного калибра (БОПС / КС / ПТУР)
* **`Shift` / Колесо мыши** — снайперский оптический прицел с баллистической сеткой и дальномером
* **`N`** — тепловизор (ТВП) наведения
* **`1 / 2 / 3`** — смена типа боеприпаса
* **`G`** — постановка дымовой завесы (СПУ 902В «Туча»)
* **`F`** — полевой ремонт модулей (гусеницы, казённик, двигатель)
* **`~` (`Ё`)** — интерактивная консоль разработчика Dagor Engine 6.5 (`help`, `ecs.info`, `vfs.ls`, `blk.reload`, `wireframe`, `skies 0..3`, `sq <код Quirrel .nut>`)
* **`F2`** — виджет телеметрии ядра Dagor Engine 6.5 (FPS, VROMFS, Quirrel VM, daSkies2)
* **`F3`** — каркасный режим рендера мешей `drv3d_DX11` (Wireframe)
* **`F4`** — переключение 4 атмосферных пресетов `daSkies2` (Закат Golden Hour / Полдень / Гроза / Ночь со звёздами)
* **`F5` (в ангаре)** — мгновенная горячая перезагрузка всех `.blk` файлов техники и камуфляжей!
