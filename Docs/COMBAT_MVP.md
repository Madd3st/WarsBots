# Combat AI MVP — 2026-10-21

Экспериментальная x64 Release DLL собрана и установлена. SHA256:
`F9F922F14177C50918C8B36FF6F17BC3D73C4265C1FB09BC3535EFFAD7787175`.
Артефакт: `build/release-bin64-probe/Mods/WarsBots/Bin64/WarsBots.dll`.
Обычная сборка остаётся OFF; AI никогда не запускается без wb_bot_start.

## Реализация

- BotBrain: SPAWNING, SEEKING_WEAPON, MOVING_TO_WEAPON, SEARCHING_ENEMY,
  CHASING_ENEMY, ATTACKING, DEAD, RESPAWNING; решения сервера до 10 раз/с.
- BotMovement: штатные CMovementRequest/IMovementController, физическое движение
  собственного активного probe, поворот/aim/fire target, лучи перед ногами/корпусом,
  проверка опоры, локальный обход и смена стороны при застревании. Телепортаций нет.
  Движение между этажами/построение глобального пути/прыжки не реализованы.
- BotWeapons: ближайшее свободное SCAR/FY71/SMG/Shotgun/SOCOM до 30 м, CanPickUp,
  PickUp и проверка владельца/инвентаря; выбор пригодного стартового оружия.
  Через максимум 6 с неудачного подхода либо при отсутствии предмета — одна тестовая
  выдача SCAR с магазином и резервом 180 за жизнь. REAL PICKUP и TEST GRANT различаются.
  Проверки патронов и перезарядка идут через штатные интерфейсы, не через изменение HP.
- BotCombat: живые реальные Player с настоящим каналом, исключение наблюдателей,
  дистанция до 80 м, LOS raycast, память цели 3 с, остановка до 18 м, aim с небольшой
  ошибкой, очередь до 0.2 с раз в 0.65 с через IWeapon::StartFire/StopFire.
- После смерти: ожидание 5 игровых секунд, существующий RevivePlayer, тот же EntityId,
  сброс намерений/оружия; при ошибке повторная попытка через 5 с без нового актёра.
  Остановка, удаление, закрытие gate и переход уровня выключают AI/возрождение.

ShotValidator ранее пытался индексировать очередь канала 0. Узкое исключение
проверяет реальный серверный non-remote projectile и его владельца, владение оружием
и зарегистрированным probe. Неподтверждённые channel-less попадания отклоняются,
клиентские попадания людей проверяются штатно. Пули в полёте допустимы после остановки
AI. Нет fake INetChannel, прямого KillPlayer/SetHealth для атаки и изменений PS.
Бот всё ещё не полноценный участник таблицы игроков; очки самого бота не интегрированы.

## Что действительно проверено

- Сборка MSVC, x64/CreateGame, установка DLL (хэши совпали), git diff --check.
- Dedicated: IA Steelmill, loopback 127.0.0.1:64100. Spawn EntityId=65488,
  BindToNetwork=true, health=100. Реальный подбор item=313, acquired=1, ammo=50.
- Телеметрия показала физическое перемещение с (1223.75,1356.75) к (1208.17,1363.12)
  и далее, без вызовов SetPos/SetWorldTM в движении.
- Клиент Magic8 подключился и загрузил IA. Он остался spectator=2, health=-1;
  бот корректно не выбрал наблюдателя противником.
- Контрольные серверные команды: старт → ранний stop → повторный start использовал
  того же актёра → remove → gate=0 → start отклонён. Один spawn, два stop, один DONE;
  нового спавна за 8 с не было. Это не проверка торможения уже движущегося бота.
- Журналы: build/combat-runtime/server-ai.log, client-ai.log, server-controls.log.
  Все запущенные агентом процессы остановлены; установленная DLL оставлена.

**Не проверены в игровом бою:** обнаружение живого Magic8, преследование, наведение,
штатные выстрелы/урон/убийство, reload, автоматический respawn после смерти,
позднее подключение. Тестовая SCAR-выдача не понадобилась: реальный pickup сработал.
Инструмент управления клиентом ранее не инициализировался; нужен ручной вход в матч.

## Запуск и ручная проверка

Из `E:\Electronic Arts\Crytek\Crysis Wars`:

```powershell
.\Bin64\CrysisWarsDedicatedServer.exe -mod WarsBots +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_gamerules InstantAction +map multiplayer/ia/steelmill
```

В консоли сервера:

```text
wb_probe_local_test 1
wb_bot_verbose 1
wb_bot_start
wb_bot_status
```

wb_bot_start без координат использует нейтральный IA spawn (или существующий probe).
Можно указать `wb_bot_start 1178 1331 135` для ранее проверенной точки Steelmill.
Клиент: `Bin64\Crysis.exe -dx9 -mod WarsBots +connect 127.0.0.1:64100`.
Войти в матч живым, подойти на открытую площадку в пределах 80 м; проверить Enemy
acquired/CHASING_ENEMY/ATTACKING, burst requested и Hit before/after-Lua с реальным
уменьшением здоровья. Затем убить бота: DEAD → 5 с → RESPAWNING, прежний EntityId,
повторный поиск оружия/атака. Повторить цикл. Проверить стоп во время движения,
стрельбы и ожидания respawn, удаление, смену карты, второго клиента.

Остановка AI: `wb_bot_stop` (актёр остаётся). Удаление: `wb_probe_remove`.
Ручные wb_probe_spawn/status/revive сохранены. Подробный лог: wb_bot_verbose 0|1.

## Файлы этой итерации

Новые: Code/Mod/Bots/BotBrain.h/.cpp, BotMovement.h/.cpp, BotWeapons.h/.cpp,
BotCombat.h/.cpp; Docs/COMBAT_MVP.md.
Изменены: CMakeLists.txt; Code/Mod/Bots/BotSpawner.h/.cpp;
Code/Mod/Player.cpp, ShotValidator.cpp, GameRulesClientServer.cpp, GameRules.cpp.
