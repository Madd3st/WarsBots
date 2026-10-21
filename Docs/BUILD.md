# Сборка на Windows

## Текущий результат, 2026-10-21

Установлены Build Tools 2022 17.14.42, MSVC 19.44.35229, Windows SDK 10.0.26100.0,
CMake 3.31.6-msvc6 и Ninja 1.12.1. В обычном shell их может не быть в PATH:
`Scripts/Build-WarsBots.ps1` активирует Developer PowerShell установленного MSVC.

Фактически выполнено `Scripts/Build-WarsBots.ps1 -Fresh` вне песочницы:
конфигурация и сборка x64 Release завершились успешно. Артефакт —
`build/release-bin64/Mods/WarsBots/Bin64/WarsBots.dll`, эксперимент выключен.
`dumpbin /headers /exports` подтвердил x64 и экспорт CreateGame.
Логи находятся в `build/release-bin64/logs`.

Первый запуск внутри песочницы был остановлен после зависания проверки ABI MSVC.
При сборке обнаружено неверное распознавание русского /showIncludes. После успешной
сборки добавлен английский пакет MSVC; скрипт задаёт VSLANG=1033. Для следующей
сборки использовать `-Fresh`, чтобы обновить определение префикса зависимостей
(требует CMake 3.24+). Повторная сборка после установки языка пока не выполнялась.
Загрузка в игру, x86/Debug и экспериментальная сборка остаются непроверенными.

## Исходное обследование до установки инструментов

Windows NT `10.0.26200.0`, DisplayVersion `25H2` (Windows 11 build; устаревшее
поле ProductName реестра сообщает Windows 10 Pro). Git `2.55.0.windows.3` работает.
`cmake`, `cl`, `ninja`, `msbuild` отсутствуют в PATH. Стандартные каталоги Visual
Studio/Build Tools, CMake и vswhere не найдены; подходящих установок нет в
просмотренном uninstall-реестре. Старые ключи VisualStudio 8–14 не доказывают наличие MSVC.

В `E:\Crysis Wars` найдены архивы, патч и установочные материалы. Во вложенной
GOG-папке Wars — два `.bin`, но не исполняемые файлы игры. Рабочая Crysis Wars 1.5
в обследованных местах не найдена; нестандартную установку это не исключает.

До изменений выполнено `cmake --preset release-bin64`: PowerShell сообщил
`The term 'cmake' is not recognized`. Конфигурация не началась, компилятор и линкер
не запускались. Финальная DLL также не собрана, загрузка не проверена.

## Требования

Windows 10+; VS 2022 / Build Tools с **Desktop development with C++**, MSVC x86/x64,
Windows SDK; CMake **3.20+** для schema 2 presets; Ninja. CMakeLists требует минимум
3.15 для ручной конфигурации без presets. Код использует C++20 и MSVC ABI;
MinGW не является заменой. Runtime статический, как в upstream.
Игра не нужна для компиляции, но полная Crysis Wars 1.5 нужна для запуска.

Установите компоненты через Visual Studio Installer. Откройте **x64 Native Tools
Command Prompt for VS 2022**, при желании запустите из него PowerShell.
Для Bin32 нужен x86 shell: presets с external architecture не переключают окружение.

```powershell
Get-Command cl, cmake, ninja
cl
cmake --version
ninja --version
```

## Контрольная сборка исходного SDK

В отдельной папке, не поверх WarsBots:

```powershell
git clone https://github.com/ccomrade/cw-mod-sdk.git cw-sdk-baseline
git -C cw-sdk-baseline checkout 98c3dad11af50c298a5564a4b50c053a5ca11bf6
cmake -S cw-sdk-baseline -B cw-sdk-baseline/build/Mods/MyMod/Bin64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl
cmake --build cw-sdk-baseline/build/Mods/MyMod/Bin64 --parallel
```

Явный `-B` сохраняет build внутри baseline. Оригинальные presets пишут в
`../Mods/MyMod`, предполагая репозиторий внутри игры. Установите контрольный MyMod
отдельно и проверьте загрузку. Сохраните логи, версию MSVC и разрядность.

## WarsBots

Из корня репозитория в x64 shell:

```powershell
cmake --list-presets
cmake --preset release-bin64
cmake --build build/release-bin64/Mods/WarsBots/Bin64 --parallel
Get-Item build/release-bin64/Mods/WarsBots/Bin64/WarsBots.dll
dumpbin /headers build/release-bin64/Mods/WarsBots/Bin64/WarsBots.dll
dumpbin /exports build/release-bin64/Mods/WarsBots/Bin64/WarsBots.dll
```

Ожидается экспорт CreateGame и архитектура процесса игры. Debug: `debug-bin64`;
x86: `release-bin32` / `debug-bin32` из x86 shell. Каждый preset имеет отдельный
`build/<preset>/Mods/WarsBots/BinXX`. При смене toolchain используйте новый build.

## Установка и запуск

Замените пример реальным путём **установленной** игры. Не копируйте весь build
и не заменяйте оригинальную игровую DLL:

```powershell
$warsGameRoot = 'C:\Games\Crysis Wars'
$warsStage = Join-Path $PWD 'build/release-bin64/Mods/WarsBots'
$warsModRoot = Join-Path $warsGameRoot 'Mods/WarsBots'
New-Item -ItemType Directory -Force -Path (Join-Path $warsModRoot 'Bin64')
Copy-Item (Join-Path $warsStage 'Bin64/WarsBots.dll') (Join-Path $warsModRoot 'Bin64/WarsBots.dll')
Copy-Item (Join-Path $warsStage 'info.xml') $warsModRoot
Copy-Item (Join-Path $warsStage 'logo.jpg') $warsModRoot
Copy-Item .\LICENSE.txt $warsModRoot
Set-Location $warsGameRoot
.\Bin64\Crysis.exe -mod WarsBots
# Отдельно после закрытия предыдущего процесса:
.\Bin64\CrysisWarsDedicatedServer.exe -mod WarsBots
```

VS Code: задайте `CRYSIS_WARS_ROOT` перед запуском редактора. Конфигурации
`.vscode/launch.json` используют эту переменную и установленный мод; автоматической
сборки/копирования нет. Локальные пути в Git не добавлять.

## Smoke test — ещё не выполнен

1. На клиенте и dedicated проверить загрузку именно WarsBots по строке
   `[WarsBots] BotManager initialized (bootstrap only; no bots spawned).` в журнале.
2. Запустить штатную IA-карту через стандартную конфигурацию сервера; подключить
   реального клиента: появление, движение, выстрел, смерть и respawn.
3. Повторить для PS: команда, появление, стандартные объекты и покупки.
4. Перезапустить матч, сменить карту, переподключить клиента, завершить игру.
   Проверить ошибки и `[WarsBots] BotManager shut down.` при штатном завершении.
   Init ожидается на жизненный цикл CGame, не на каждую карту.
5. Записать версию игры/launcher, x86/x64, карты и журналы. Отдельно проверить
   немодифицированного клиента: server-only установка ещё не доказана.

Статические проверки не заменяют компиляцию, линковку и загрузку движком.
