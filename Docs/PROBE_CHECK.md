# Результат локальной проверки, 2026-10-21

- `Scripts/Build-WarsBots.ps1 -ExperimentalSpawner`: x64 Release, успешно;
  экспорт CreateGame проверен. DLL установлена в игру, SHA256:
  `00231148159D31BBAD7EE7F4621EBB4E18E8FA3F27E1B0A3D239ADF3A3C9151D`.
- Сервер слушал только `127.0.0.1:64100`, `sv_lanonly=1`.
  IA Steelmill загрузилась за 0.44 с. Первый клиент подключился и загрузил карту
  за 3.79 с; журнал подтвердил `Game rules class: InstantAction`.
- Runtime-флаг по умолчанию 0. Команд создания не отправляли, автоматического
  спавна в коде нет; в журналах нет Probe created/BindToNetwork result.
- Второй процесс клиента завершился с кодом 1 до создания отдельного журнала.
  Второе подключение и причина завершения не подтверждены.
- Управление окнами недоступно: node_repl дважды сообщил `failed to write kernel assets`.
  Ручной спавн, результат BindToNetwork, удаление и видимость НЕ ПРОВЕРЕНЫ.
  Требование изменений закрытого CryEngine пока не установлено.
- Журналы: `build/release-bin64-probe/logs/server-ia.log`, `client1-ia.log`, `build.log`.
  Тестовые процессы остановлены принудительно; это не тест штатной очистки.

## Продолжение вручную

Из `E:\Electronic Arts\Crytek\Crysis Wars` запустить сервер без +quit:

```powershell
.\Bin64\CrysisWarsDedicatedServer.exe -mod WarsBots +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_gamerules InstantAction +map multiplayer/ia/steelmill
```

Клиент: `Bin64\Crysis.exe -dx9 -mod WarsBots +connect 127.0.0.1:64100`.
После загрузки карты в серверной консоли вручную:

```text
wb_probe_local_test 1
wb_probe_spawn X Y Z
wb_probe_status
wb_probe_remove
```

X/Y/Z заменить свободной видимой точкой рядом с игроком; 0 0 0 не универсальный спавн.
Создание требует активного MP-уровня, локального bind и LAN-режима. Повторный спавн
блокируется до удаления собственного актёра; чужую сущность с тем же именем не удаляет.
Логирует фактический BindToNetwork в Actor::Init без повторного binding, EntityId,
класс и позицию. Стандартная логика BindToNetwork сохраняется.

Обычная сборка по-прежнему имеет CMake-флаг OFF. В установленной экспериментальной
сборке `wb_probe_local_test` по умолчанию 0; никакого автоматического создания нет.
Для следующего этапа: увидеть актёра первым клиентом, затем подключить независимый
второй клиент и проверить late join, удалить/создать повторно и проверить смену карты.
