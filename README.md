# Crysis Wars Bots / WarsBots

Серверные мультиплеерные боты для Crysis Wars 1.5: сначала Instant Action,
затем Power Struggle. Сейчас реализован экспериментальный **Combat AI MVP для локальной IA**.
Управление выполняется через C++ игровую логику, без эмуляции мыши и клавиатуры.

Основа: [ccomrade/cw-mod-sdk](https://github.com/ccomrade/cw-mod-sdk),
коммит `98c3dad11af50c298a5564a4b50c053a5ca11bf6`. История SDK сохранена.
Исходный README сохранён в [UPSTREAM_README.md](UPSTREAM_README.md).

**Для нового агента:** начните с [AGENTS.md](AGENTS.md) (карта кода и правила)
и [Docs/STATUS.md](Docs/STATUS.md) (актуальные результаты и открытые проверки).
Документы ранних этапов сохраняются как история.

## Текущее состояние

- `WarsBots::BotManager` принадлежит `CGame`, инициализируется в конце `CGame::Init`
  и завершается до освобождения игровых систем в деструкторе.
- Ожидаемый лог: `[WarsBots] BotManager initialized (bootstrap only; no bots spawned).`
- Сохранены ручные создание, удаление и возрождение актёра без фиктивного сетевого канала.
- Combat AI включает поиск оружия/противника, локальное движение, наведение, штатную
  стрельбу и автоматическое возрождение; по умолчанию выключен.
- **Обычная и экспериментальная DLL собраны: x64 Release**, экспорт `CreateGame`
  проверен. Загрузка мода подтверждена в клиенте и dedicated server `1.1.1.6729`.
- На IA Steelmill проверены явный запуск бота, подбор оружия, движение,
  подключение клиента, обнаружение живого Magic8, штатный урон/убийство и respawn
  того же актёра в другой точке. Визуальная анимация, точные LOS-сценарии и второй
  клиент требуют отдельных проверок. Полноценное участие бота в статистике не реализовано.
- Результаты и команды: [Combat AI MVP](Docs/COMBAT_MVP.md).

## Сборка и запуск

После установки MSVC, Windows SDK, CMake и Ninja, из x64 Native Tools shell:

```powershell
cmake --preset release-bin64
cmake --build build/release-bin64/Mods/WarsBots/Bin64 --parallel
```

Экспериментальная версия (Windows PowerShell, установленные VS Build Tools):

```powershell
.\Scripts\Build-WarsBots.ps1 -ExperimentalSpawner
```

Артефакт: `build/release-bin64-probe/Mods/WarsBots/Bin64/WarsBots.dll`.
AI разрешён только на локальном IA-сервере с `sv_bind=127.0.0.1`, `sv_lanonly=1`.
После загрузки карты в серверной консоли: `wb_probe_local_test 1`, затем
`wb_bot_start`. Остановка: `wb_bot_stop`; состояние: `wb_bot_status`.

Скопируйте DLL и метаданные в `Crysis Wars/Mods/WarsBots` по инструкции
[BUILD](Docs/BUILD.md), затем из папки игры:

```powershell
.\Bin64\Crysis.exe -mod WarsBots
# Или выделенный сервер:
.\Bin64\CrysisWarsDedicatedServer.exe -mod WarsBots
```

## Структура

```text
Code/CryEngine/          интерфейсы CryEngine из SDK
Code/Mod/               исходная игровая логика SDK
Code/Mod/Bots/          BotManager, BotSpawner, BotBrain, BotMovement, BotWeapons, BotCombat
Scripts/               воспроизводимая MSVC-сборка
Docs/
  STATUS.md             актуальная передача контекста следующему агенту
  ARCHITECTURE.md       компоненты и решения
  BUILD.md              сборка, установка, ручная проверка
  RESEARCH.md           механизмы SDK, доказательства и неизвестные
  ROADMAP.md            последовательность разработки
  RISKS.md              ограничения
  VALIDATION.md         результаты проверок и список изменений
  COMBAT_MVP.md         актуальное состояние боевого прототипа и ручной тест
CMakeLists.txt          DLL-цель WarsBots
CMakePresets.json       x86/x64, Debug/Release, локальный staging
AGENTS.md               правила работы и карта кода для агентов
LICENSE.txt            неизменённая лицензия Crytek MOD SDK
```

Сохранено существующее имя папки `Docs` SDK (на Windows доступно также как `docs`).
Игровые EXE, PAK, DLL и сторонние бинарные библиотеки в Git не добавляются.

## Лицензирование

Исходники доступны для изучения, но SDK не имеет свободной MIT/GPL-лицензии:
сохраняется [Crytek Limited License Agreement](LICENSE.txt), включая ограничения
использования и распространения. Не следует объявлять весь производный проект
свободным open-source или менять лицензию SDK. Код референсных проектов не копировался.
WarsBots — собственная модификация, не продукт Crytek или Electronic Arts.

This site is not endorsed by or affiliated with Crytek or Electronic Arts. Trademarks are the property of their respective owners. Game content copyright Crytek.
