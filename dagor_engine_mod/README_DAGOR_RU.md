# Нативный модуль Steel Front: Online под Gaijin Dagor Engine (`daNetGame`)

Эта папка содержит исходный проект **Steel Front: Online** под официальный открытый движок **Dagor Engine** от Gaijin Entertainment (`https://github.com/GaijinEntertainment/DagorEngine`, на базе которого созданы *War Thunder*, *Enlisted* и *Crossout*).

## Что входит в `steelFrontOnline/`

- `application.blk` — конфигурация приложения Dagor Engine (DX12 / Vulkan, разрешения, монтирование `%gameBase` и `%danetlibs`).
- `prog/jamfile` и `prog/build.py` — скрипты сборки нативного клиента (`.exe`) и выделенного онлайн-сервера (`-sDedicated=yes`) с AOT-компиляцией `daScript`.
- `prog/main/main.cpp` — C++ точка входа и регистрация игровых танковых модулей `daNetGame`.
- `prog/gameBase/gamedata/units/tanks_serbia_ru_nato.blk` — шаблоны ECS-сущностей бронетехники всех 7 наций, включая **Сербию** (`M-84AS2 Војвода`, `M-84AS1`, `M-91 Вихор`, `BVP M-80AB1`), Россию (`Т-80У`, `Т-80БВМ`, `Т-90М`), Китай (`ZTZ-99A`), США (`M1A2 SEP v3`), НАТО/ЕС (`Leopard 2A7V`), Францию (`Leclerc SXXI XLR`) и Швецию (`Strv 122B+`).
- `prog/gameBase/gamedata/weapons/cannons_120_125mm.blk` — баллистические параметры снарядов ОБПС, КС, ОФС и тандемных ПТУР (`Маљутка-2Т5`, `9М119М1`, `Рефлекс-М`).
- `prog/scripts/steel_front/es/*.das` — боевые серверные и клиентские системы на языке **DaScript**:
  - `player_session.das` — подключение игроков и реплицируемый спавн танка;
  - `tank_chassis_phys.das` — двухкруговое наведение башни и орудия за курсором мыши (Mouse Aim);
  - `armor_ballistics_dm.das` — расчёт приведённой брони, ДЗ, рикошетов и детонации боеукладки;
  - `conquest_zones.das` — онлайн-захват точек `[A / B / C]`.
- `prog/scripts/ui/hangar_wt.nut` — интерфейс ангара в стиле War Thunder на фреймворке **darg / Quirrel**.

## Как собрать на Windows 10/11 x64

1. Убедитесь, что установлены **Git**, **Python 3**, **7-Zip** и свободно **150–200 ГБ** на диске.
2. Запустите двойным кликом **`SETUP_AND_BUILD_DAGOR_WIN64.bat`**.
3. Скрипт скачает `GaijinEntertainment/DagorEngine`, настроит `devtools` (`MSVC`, `Clang`, `DX12 Agility SDK`, `jam`), распакует `tools-base.7z` при необходимости и соберёт клиент + выделенный онлайн-сервер.

> **Хотите играть онлайн прямо сейчас за 3 секунды без скачивания 200 ГБ?**  
> Вернитесь в корневую папку репозитория и запустите **`START_ONLINE_GAME.bat`**!
