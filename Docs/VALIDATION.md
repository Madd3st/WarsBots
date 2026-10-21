# Результаты подготовки и проверок

Дата: 2026-10-21. Проверки выполнены в Windows workspace пользователя.

## Этап 3: фактическая загрузка мода

Игра установлена в `E:\Electronic Arts\Crytek\Crysis Wars`, версия EXE `1.1.1.6729`.
У клиента и dedicated server проверены PE machine: Bin32 — 014C (x86),
Bin64 — 8664 (x64). В `Mods/WarsBots` установлены info.xml, logo.jpg, LICENSE.txt
и `Bin64/WarsBots.dll`; SHA256 совпадает с собранной DLL этапа 2.
Экспериментальный spawner остаётся OFF, игровая логика не изменялась.

Из папки игры выполнено `Bin64\CrysisWarsDedicatedServer.exe -mod WarsBots +quit`.
Процесс завершился с кодом 0. В Server.log от 20:35:32 подтверждены мод WarsBots,
64-bit executable и `[WarsBots] BotManager initialized (bootstrap only; no bots spawned).`.
Ошибок загрузки в журнале нет. Снимок: `build/release-bin64/logs/server-load.log`.
Сообщение shutdown менеджера в этом запуске не записано; подтверждена инициализация.
Клиентский запуск, матч и сетевые проверки не выполнялись.

Для ручного запуска из папки игры: `Bin64\Crysis.exe -dx9 -mod WarsBots` либо
`Bin64\CrysisWarsDedicatedServer.exe -mod WarsBots` без `+quit`.
Для конфигураций VS Code задать CRYSIS_WARS_ROOT равным найденному каталогу игры.
Ниже приведены результаты предшествующих этапов; их ограничения по загрузке
относятся к состоянию до данного теста.

## Этап 2: подтверждённая сборка WarsBots

- Установлены Build Tools 2022 17.14.42, MSVC 19.44.35229, Windows SDK 10.0.26100.0,
  CMake 3.31.6-msvc6 и Ninja 1.12.1.
- Успешная команда: `Scripts/Build-WarsBots.ps1 -Fresh` вне песочницы. Скрипт выполнил
  `cmake --preset release-bin64 -B <workspace>/build/release-bin64/Mods/WarsBots/Bin64 -DWARSBOTS_EXPERIMENTAL_SPAWNER=OFF --fresh`,
  затем `cmake --build <тот же каталог> --parallel 4` и `dumpbin /headers /exports`.
- Результат: x64 Release, `WarsBots.dll`, 3 884 032 байта; экспорт CreateGame найден.
  SHA256: `609D9A77EE0F922BACC443E37940D2446BAF9F95AD5F5CE89C675C57A937CB9B`.
- Логи: `build/release-bin64/logs/configure.log`, `build.log`, `dll-inspection.log`.
  Окончание сборки: `[198/198] Linking CXX shared module WarsBots.dll`; скрипт завершился с кодом 0.
- Первая попытка в песочнице зависла на проверке ABI и была остановлена. Первые
  внешние попытки остановлены при устранении локализации /showIncludes; итоговая
  сборка выше успешна. Английский пакет установлен после неё, код установщика 0.
  Последующая инкрементальная сборка с обновлённой локализацией не проверялась.
- Загрузка DLL движком, отсутствие игровых регрессий, репликация и утечки: **NOT RUN**.
  Чистый upstream отдельно не собран. x86, Debug и экспериментальный BotSpawner не собраны.

Изменения этапа 2: флаг CMake OFF по умолчанию; передача framework в BotManager;
ранний Shutdown перед освобождением storage; `Scripts/Build-WarsBots.ps1`;
экспериментальные `BotSpawner.h/.cpp` и `Docs/NETWORK_TESTS.md`, дополнение RESEARCH.
Исходники BotSpawner уже подготовлены, но исключены из подтверждённой сборки.
По текущему приоритету их дальнейшая разработка и проверка отложены.

Ниже сохранён исторический результат первого этапа, до установки инструментов.

## Git и исходная база

До начала: пустой рабочий каталог и существующий `.git`, без коммитов и remotes.
В этот же репозиторий загружена история cw-mod-sdk, создана ветка
`codex/warsbots-bootstrap` от `98c3dad11af50c298a5564a4b50c053a5ca11bf6`.
Исходная `.git` не удалялась и не переинициализировалась. Изменения WarsBots оставлены
в рабочем дереве для просмотра, отдельный commit и публикация не выполнялись.

## Проверки этапа 1 (история)

| Проверка | Результат |
| --- | --- |
| Чистый SDK: `cmake --preset release-bin64` до изменения кода | Не стартовал: команда cmake не найдена |
| Компиляция / линковка чистого SDK | Не выполнены, MSVC не найден |
| Компиляция / линковка WarsBots | Не выполнены по той же причине |
| Загрузка DLL в клиент / dedicated | Не выполнена: нет DLL и обнаруженной рабочей установки |
| `git diff --check` | Успех; только информационное предупреждение LF→CRLF для README |
| JSON-разбор CMakePresets.json | Успех: 6 configure presets, из них 2 скрытых базовых |
| JSON-разбор .vscode/launch.json | Успех: 12 конфигураций запуска |
| Существование перечисленных CMake-путей | 200 файлов, отсутствующих 0; включая BotManager.cpp |
| Сохранность LICENSE.txt | Git blob hash совпал с базой: `55528205a98a37ed8420b7de27b2edda67b6e5f4` |
| Сохранность исходного README | UPSTREAM_README.md совпал с базой: `a03775c52c9cf6c278fef2d58aa3b7315c5f2e9f` |
| Игровые бинарные файлы в tracked snapshot | `.dll`, `.exe`, `.lib`, `.pak`, `.bin` отсутствуют |
| Правила ignore | Проверены для build/test/WarsBots.dll, local/Game/Scripts.pak, sample.exe, sample.lib |
| Локальные ссылки Markdown | Проверены README, UPSTREAM_README и Docs/*.md; отсутствующих целей 0 |

JSON-разбор не является исполнением CMake. Проверка путей не является проверкой
компилируемости. Сохранение исходных GameRules не является runtime regression test.
Все пункты smoke test из BUILD остаются открытыми.

## Изменённые файлы относительно базы

| Файл | Изменение |
| --- | --- |
| `.gitignore` | Исключения для build, игровых бинарных файлов, архивов и local |
| `.vscode/launch.json` | WarsBots и CRYSIS_WARS_ROOT вместо жёсткой привязки к родительской папке |
| `CMakeLists.txt` | Имя WarsBots, версия 0.1.0.0, описание, включение BotManager.cpp |
| `CMakePresets.json` | Staging внутри build/<preset>/Mods/WarsBots, разделение конфигураций |
| `Code/Mod/Game.cpp` | Только вызовы Initialize и Shutdown менеджера |
| `Code/Mod/Game.h` | Include и член BotManager по значению |
| `README.md` | Назначение, статус, структура, запуск и лицензирование WarsBots |

Новые файлы: `Code/Mod/Bots/BotManager.h`, `Code/Mod/Bots/BotManager.cpp`,
`Docs/ARCHITECTURE.md`, `Docs/BUILD.md`, `Docs/RESEARCH.md`, `Docs/ROADMAP.md`,
`Docs/RISKS.md`, `Docs/VALIDATION.md`, `UPSTREAM_README.md`.

Никаких скриптов игры, сторонних реализаций Co-op/AStopher и бинарных библиотек
не добавлено. Существующие изображения документации и logo.jpg пришли из SDK.

## Исходный технический результат этапа 1 (история)

Исходный код минимального мода подготовлен: реальная DLL-цель CMake и вызов
BotManager из lifecycle SDK. Логирование реализовано через существующий CryLogAlways.
Подтверждение компиляции, загрузки и отсутствия регрессий отложено до доступного
MSVC и рабочей игры. Этап «работающий минимальный мод» пока не считается завершённым.
