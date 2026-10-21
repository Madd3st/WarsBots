# Передача прогресса: abilities, 2026-10-21

Ветка `codex/warsbots-abilities`, база `b62ef3a` из bootstrap. Работа сохранена
по просьбе пользователя из-за остатка лимита. Фаза НЕ объявлена завершённой.
Наш последний тестовый сервер остановлен; при сохранении клиентов не было.

## Что сделано

- Добавлены BotBlackboard, BotIntent, BotLocomotion, BotSuit. BotBrain формирует
  намерение; движение и костюм исполняются через существующие механизмы движка.
- Walk/run/sprint, штатные normal/Strength jump, проверка места прыжка,
  cooldown и наблюдение фактического взлёта/приземления. Нет телепорта/импульсов.
- Штатные NanoSuit mode/energy/recharge, ограничения энергии и частоты переключений.
  Cloak изолирован, `wb_bot_cloak_experimental=0` по умолчанию.
- Sprint/stance/airborne/jump sequence передаются клиенту; клиент не создаёт
  собственный физический прыжок. Сервер и клиенты требуют одинаковую DLL.
- Собственный актёр получает корректную инициализацию костюма; обычные игроки
  защищены проверкой принадлежности WarsBots. Фиктивных каналов нет.
- Stop/death/respawn сбрасывают намерения и отложенный прыжок; существующие
  оружие, бой и respawn сохранены. Полная регрессия этой версии ещё нужна.
- Исправлен реальный краш первого клиента: SerializeSpawnInfo вызывается ДО Init.
  Там допустимо только сохранить marker; нельзя обращаться к GameObject,
  GetChannelId или костюму. Инициализация перенесена в подходящий этап Init/Revive.

## Что проверено фактически

- MSVC x64 Release experimental и default OFF собраны, экспорт CreateGame проверен.
  Команды: `Scripts/Build-WarsBots.ps1 -ExperimentalSpawner` и без параметра.
- Финальная experimental DLL собрана и установлена:
  `build/release-bin64-probe/Mods/WarsBots/Bin64/WarsBots.dll`;
  `E:\Electronic Arts\Crytek\Crysis Wars\Mods\WarsBots\Bin64\WarsBots.dll`.
  SHA256: `4E3F1E9B42F48C32251F89943175A847770D13ADC519D8532FE6796FAD64654B`.
- Сервер IA Steelmill: walk 1.30, run 2.50, sprint 5.46 м/с. Энергия спринта
  убывает, затем штатно восстанавливается до 200.
- Normal jump: vz 4.43 м/с, подъём 0.89 м; Strength: vz 9.60, подъём 3.63 м,
  энергия при взлёте около 130.7 из 200. Физическое приземление записано.
- Один реальный клиент после исправления подключился и получил костюм active=1,
  смену mode/energy и airborne/jump sequence. Пользователь сам закрыл одно окно;
  System:Quit этого клиента не является доказательством краша.
- Закрытый локальный gate и выключенный cloak отклоняют команды. Stop/remove
  отменяют ожидающий прыжок; ENTITY_EVENT_DONE получен. Финальный smoke:
  start/stop/start, настоящий pickup acquired=1, движение и костюм без клиента.
- Архивы только локально, игнорируются Git: `build/abilities-runtime/`:
  server-sprint, server-normal-jump, server-strength-jump, server-walk-gates,
  server-network-suit, client1-network-suit, server-stop-remove, server-final-smoke.
  Исторический client-spawn-order-failure относится к исправленной версии.
- `Scripts/Test-WarsBotsAbilityLogs.ps1` проверяет эти журналы; предыдущий прогон
  прошёл 15 проверок. Последняя небольшая правка самого скрипта не перепроверялась.

## Что не доделано / следующий шаг

- Визуально проверить gait, прыжки, приземление/idle, костюм и отсутствие рывков.
  UI-инструмент не запускался из-за ошибки создания kernel assets; логи не заменяют зрение.
- Два одновременных клиента НЕ проверены: второго канала/отдельного журнала нет.
  Причина закрытия второго процесса не доказана; не объявлять сетевой тест пройденным.
  Локальный gate требует loopback, не ослаблять его ради подключения другого ПК.
- Провести регрессию реального боя, смерти и respawn с новой версией костюма;
  базовый боевой тест был до этой фазы. Cloak визуально/по сети не подтверждён.
- channelId=0, IsPlayer=false, участник матча NO. Статистика IA и PS не исправлялись.

## Ручное продолжение

Сервер и подключение клиента: команды в [STATUS](STATUS.md). После карты:
`wb_probe_local_test 1`, `wb_bot_verbose 1`, `wb_bot_start`.
Диагностика: `wb_bot_abilities`. Тесты: `wb_bot_gait walk|run|sprint|auto`,
`wb_bot_jump normal|strength [delaySeconds]`,
`wb_bot_suit defense|speed|strength|cloak|auto` (выбрать одно значение).
Остановка `wb_bot_stop`, удаление `wb_probe_remove`. Автоспавна нет.
Не завершать пользовательскую сессию без согласования и не применять `+quit`.
