# Проверка жизненного цикла probe

Создание, видимость, убийство человеком и повторное создание подтверждены пользователем.
В этой итерации добавлена диагностика и ручное возрождение; игровой прогон новой DLL
ещё не выполнен. Автоматического спавна/возрождения и изменения начисления очков нет.

## Что найдено в установленной IA 1.5

Изучены только нужные функции Lua из `Game/ZPatch1.pak`, ресурсы не импортировались:
`SinglePlayer.Server:OnHit` → `BasicActor.Server:OnHit` →
`InstantAction:ProcessActorDamage` → при смерти `ProcessDeath` →
`InstantAction.Server:OnPlayerKilled` → C++ `GameRules::KillPlayer` и `ProcessScores`.
ProcessServerHit предварительно проверяет shot validator, затем вызывает Lua OnHit.

C++ KillPlayer вызывает NetKill/ClKill и GameplayRecorder eGE_Death, а для существующего
стрелка, отличного от жертвы, — eGE_Kill. NetKill вызывает GameRules::OnKill/OnKillMessage.
Сами recorder events не доказывают начисления очков.

`InstantAction.lua::ProcessScores` (1318–1341): смерть жертвы записывается только при
target.actor:IsPlayer(); убийство реального стрелка начисляется независимо от IsPlayer
жертвы. Поэтому channelId=0 не исключает kill credit стрелка. В PostGame начисление
выключено. Клиентская OnKill отдельно фильтрует IsPlayer жертвы для части HUD-уведомлений.
Отсутствие сообщения на экране не доказывает отсутствие очка.

Новая диагностика before-Lua/after-Lua читает IA keys 100 (убийства стрелка) и 101
(смерти probe); valid=0 означает отсутствие значения, а не подтверждённый нулевой счёт.
Сравнение этих записей на смертельном попадании и scoreboard — ручная проверка.

## Возрождение

`wb_probe_revive` вызывает штатный GameRules::RevivePlayer для того же живого объекта
с health<=0; позиция/углы берутся из исходного спавна. NetReviveAt восстанавливает
здоровье, Revive сбрасывает физику/движение, ClRevive уведомляет клиентов.
Для собственного channelId=0 probe в IA пропускается закрытый IsChannelOnHold(0):
сетевого канала нет. Для обычных актёров и PS условие остаётся штатным.

Команда требует включённого локального IA-теста, собственного мёртвого Player с
inventory, без заморозки/техники. Удалённая сущность не создаётся заново; живой probe
не возрождается повторно. Серверный результат проверяет EntityId/identity, health
и physics profile. Это ещё не доказательство корректного отображения у клиента.

## Ручная проверка новой DLL

Остановить игру/сервер, заменить установленную Mods/WarsBots/Bin64/WarsBots.dll
артефактом `build/release-bin64-probe/Mods/WarsBots/Bin64/WarsBots.dll`, запустить
локальную IA с sv_bind=127.0.0.1 и sv_lanonly=1. В серверной консоли:

```text
wb_probe_local_test 1
wb_probe_spawn 1178 1331 135
wb_probe_status
```

Координаты — ранее проверенная точка пользователя на Steelmill; для другой карты
выбрать свободное место. Записать EntityId и счёт стрелка, убить probe человеком.
Проверить Hit before/after, GameRules::KillPlayer, Death observed и счёт в таблице.
Затем:

```text
wb_probe_status
wb_probe_revive
wb_probe_status
```

Проверить прежний EntityId, health>0, alive=1 и стоящую модель у клиента. Повторить
убийство/возрождение 3 раза. Повторный revive живого должен отказать. После
wb_probe_remove дождаться ENTITY_EVENT_DONE: revive должен отказать, новый spawn
должен работать. Также проверить рестарт/смену карты и позднее подключение клиента.
Если corpse удалён движком, не считать отказ revive ошибкой владения: сущность уже
не существует. LastHitShooter в диагностике не является доказательством убийцы;
точный killer берётся из GameRules::KillPlayer.
