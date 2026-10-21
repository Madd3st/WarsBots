# Внешнее исследование AI-ботов для FPS и применимость к WarsBots

Дата: 2026-10-21. Ветка: `codex/warsbots-abilities`.
Статус: **только исследование и проектирование**. В этом изменении нет кода,
зависимостей, новых DLL и изменений игровых ресурсов.

## 0. Краткий вывод

1. **Разделение «решение → намерение → исполнение» уже выбрано правильно.**
   `BotIntent` + `BotBrain` (10 Гц) + `BotLocomotion`/`BotMovement`/`BotCombat`
   ближе к удачным местам Q3 BotLib и YaPB, чем к монолитным `Bot::Think`.
   Следующий шаг — не менять схему, а формализовать её: приоритетный арбитр
   намерений, структурированный результат действия и узкий интерфейс к миру,
   чтобы логику можно было тестировать без движка.

2. **Ни CryAI-навигация, ни Recast/Detour не являются следующим шагом.**
   На Steelmill нет пригодных навигационных данных: в `level.pak` лежат
   пустые `*.bai` (нулевые счётчики), а движковый путь требует
   `gEnv->bMultiplayer=false` и повторной `pAISystem->Init()`, что уже
   запрещено решением 7 в `ARCHITECTURE.md`. Recast технически возможен,
   но его главная стоимость — не библиотека, а извлечение геометрии,
   генерация jump/off-mesh связей и offline-пайплайн ассетов.
   Разумный порядок: собственный waypoint-граф с типизированными рёбрами и A*,
   затем — при доказанной нехватке — спайк Recast с жёсткими критериями.

3. **Самое дешёвое улучшение поведения — восприятие и выбор цели.**
   Сейчас `BotCombat::FindEnemy` выбирает ближайшего видимого врага, помнит
   позицию 3 с и не получает стимулов (урон, звук). Готовые и проверенные
   схемы: `SensoryMemory`/`MemoryRecord` (Omni-bot), `CBotVisibles`
   (RCBot2), `EngagementScore`/`ShouldSwitchFocus` (YaPB). Это 2–4 дня
   работы с наблюдаемым эффектом и без новых зависимостей.

## 1. Метод и границы

Читались **исходники**, а не README: функции планирования, восприятия,
навигации, выбора оружия и сборки navmesh. Коммиты зафиксированы, ссылки ниже
ведут на конкретные строки.

| Проект | Зафиксированный коммит | Что именно читалось |
| --- | --- | --- |
| Quake III Arena | [`dbe4ddb`](https://github.com/id-Software/Quake-III-Arena/tree/dbe4ddb10315479fc00086f08e25d968b4b43c49) | `be_aas_route.c`, `be_aas_reach.c`, `be_ai_move.c`, `be_ai_weap.c`, `be_ai_weight.c`, `be_ai_goal.c`, `ai_dmq3.c`, `botlib.h`, `be_interface.c` |
| YaPB | [`7985904`](https://github.com/yapb/yapb/tree/7985904584a0755b216e17cc5de8c5fd28953895) | `planner.cpp`, `graph.h`, `tasks.h`, `combat.cpp`, `vision.cpp`, `weapons.cpp`, `navigate.cpp`, `tests/host/*` |
| RCBot2 | [`afa2d4e`](https://github.com/APGRoboCop/rcbot2/tree/afa2d4e3f172f2e097036f1550b37230de48ecdf) | `bot_utility.*`, `bot_waypoint.cpp`, `bot_profile.h`, `bot_visibles.cpp`, `bot_navmesh.cpp`, `bot_belief.h`, `bot_som.h` |
| Omni-bot | [`c2f664a`](https://github.com/jswigart/omni-bot/tree/c2f664a9b03202e78c5260ac0924b467dff558d3) | `StateMachine.*`, `BotSensoryMemory.*`, `MemoryRecord.*`, `BotTargetingSystem.*`, `BotWeaponSystem.*`, `BotSteeringSystem.*`, `PathPlannerRecast.*`, `RecastInterfaces.*`, `CollisionModel.h`, `GOAP.cpp`, `TacticalManager.*`, `MovementCaps.h` |
| Recast/Detour | [`9f4ce64`](https://github.com/recastnavigation/recastnavigation/tree/9f4ce64458dfae86e1239c525ddc219c4e9e06f1) | `Recast.h`, `DetourNavMesh.h`, `DetourNavMeshQuery.cpp`, `DetourPathCorridor.cpp`, `DetourCrowd.cpp` |
| Crysis Co-op | [`138cf44`](https://github.com/MikeCrossley/Crysis-Co-op/tree/138cf44e0fa134415e24ff19ff299e7198387733) | `CoopSystem.cpp`, `AIDemoInput.h`, `Game/Scripts/Coop/AI.lua` |

Дополнительно проверялась **установленная игра** (не только репозиторий):
`E:\Electronic Arts\Crytek\Crysis Wars`, карта `multiplayer/ia/steelmill`.

Что **не** проверялось в этой задаче:

- внешние боты не запускались, их поведение не измерялось;
- Recast/Detour не собирались и не линковались с WarsBots;
- навигационная геометрия Steelmill не извлекалась;
- выводы о производительности на N ботах — оценки, а не замеры.

## 2. Сводка проектов

| Проект | Лицензия | Навигация | Модель решений | Ценность для WarsBots |
| --- | --- | --- | --- | --- |
| Q3 BotLib | GPL-2.0 | AAS: области + reachability + кластерные кэши | state machine + goal weights + travel flags | эталон разделения travel-возможностей и исполнения |
| YaPB | Unlicense | waypoint-граф, A* со сменными эвристиками | стек задач с desire/priority | задачи, приоритеты, оценка цели, aim bias |
| RCBot2 | **AGPL-3.0** | waypoint-граф, A*; navmesh-навигатор **заглушён** | utility + профили | профили сложности, видимость по PVS |
| Omni-bot | корневой лицензии нет; часть GPL | Recast/Detour + flood-fill из BSP | иерархическая state machine + фильтры | сенсорная память, фильтры цели, приоритеты оружия |
| Recast/Detour | Zlib | — | — | возможная основа navmesh, но не «из коробки» |
| Crysis Co-op | лицензии нет; MOD SDK EULA | движковый CryAI | — | подтверждение цены движкового AI в MP |

Лицензии разобраны отдельно в §9: **AGPL-3.0 у RCBot2** исключает перенос
кода, а у Omni-bot и Crysis Co-op вообще нет корневой лицензии.

## 3. Организация AI: решения отдельно от исполнения

### 3.1 Quake III: библиотека за узким интерфейсом

BotLib не знает об игре напрямую: он получает таблицу импорта
`botlib_import_t` ([`botlib.h#L169-L205`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/game/botlib.h#L169-L205))
и отдаёт наружу `GetBotLibAPI` ([`be_interface.c#L848`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_interface.c#L848)).
Все обращения к физике, времени, entity и печати идут через `trap_*`.
Это даёт две вещи, которых WarsBots сейчас не хватает:

- **тестируемость решений** без загрузки уровня;
- **явную границу**: решение не может мутировать физику «случайно».

Внутри — классический цикл `BotAI` → состояние (`AIEnter_*`) → выбор цели
(`BotChooseNBGItem`) → движение (`BotMoveToGoal`) → бой (`BotCheckAttack`).
Намерение как отдельная сущность не выделено: состояние само вызывает
исполнителей, а результат приходит структурой `bot_moveresult_t`
(`failure`, `blocked`, `traveltype`), а не `bool`.

### 3.2 YaPB: стек задач с желанием и приоритетом

YaPB формализует то, что WarsBots пока делает ветвлениями в `BotBrain::Update`:

- `TaskId` — перечислимые задачи (`MoveTo`, `Attack`, `SeekCover`,
  `PickupItem`, `Camp`, …), [`tasks.h#L13`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/inc/tasks.h#L13);
- `TaskPri` — «жёсткие» желания (`kAttack=90`, `kSeekCover=91`,
  `kThrow=99`), [`tasks.h#L40`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/inc/tasks.h#L40);
- `Task` хранит функцию, `desire`, данные, время и флаг `resume`
  ([`tasks.h#L60`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/inc/tasks.h#L60));
- `TaskStack` умеет искать задачу по id и поддерживает приостановку/возврат
  ([`tasks.h#L77`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/inc/tasks.h#L77)).

Важная деталь: приоритет — не только «что важнее», но и **что можно
приостановить и возобновить**. Именно этого не хватает текущему
`BotBrain`: при появлении врага патруль заменяется, но прежняя задача
не сохраняется и не восстанавливается.

### 3.3 RCBot2: utility-оценка и профиль

`CBotUtility` хранит одно число полезности и умеет сравниваться
([`bot_utility.h#L164-L185`](https://github.com/APGRoboCop/rcbot2/blob/afa2d4e3f172f2e097036f1550b37230de48ecdf/utils/RCBot2_meta/bot_utility.h#L164-L185));
`CBotUtilities::nextBest()` выбирает максимум
([`#L202-L217`](https://github.com/APGRoboCop/rcbot2/blob/afa2d4e3f172f2e097036f1550b37230de48ecdf/utils/RCBot2_meta/bot_utility.h#L202-L217)).
Полезность не константа: к ней добавляется случайное значение `min/max`
из таблиц полезности по классам (в RCBot2 — TF2)
([`bot_utility.cpp#L177-L179`](https://github.com/APGRoboCop/rcbot2/blob/afa2d4e3f172f2e097036f1550b37230de48ecdf/utils/RCBot2_meta/bot_utility.cpp#L177-L179)).
Это дешёвый способ получить разнообразие поведения без кода на каждый случай.

### 3.4 Omni-bot: иерархическая state machine, а не GOAP

Важная отрицательная находка: `GOAP.cpp` — **заглушка** на 326 байт,
единственный `GOAP_MoveTo` с пустыми методами. Нельзя ссылаться на
«GOAP Omni-bot» как на работающую архитектуру.

Реально используется иерархия состояний:
`StatePrioritized`, `StateSequential`, `StateSequentialLooping`,
`StateSimultaneous`, `StateProbabilistic`, `StateChild`
([`StateMachine.h#L530-L665`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/StateMachine.h#L530-L665)).
`State::Update` возвращает статус, а не `bool`; состояние может быть
вложенным и иметь собственный `UpdateDelay`. Вокруг — независимые системы:
сенсорная память, targeting, weapon, steering, tactical. Это ближе к
«несколько подсистем + арбитр», чем к одному большому `switch`.

### 3.5 Что есть в WarsBots

- `BotIntent` — чистое значение: `move`, `destination`, `aim`, `gait`,
  `jump`, `stance`, `suitMode`, `combat` (`Code/Mod/Bots/BotIntent.h`).
- `BotBrain::Update` принимает решения на 10 Гц и заполняет `BotIntent`
  (`Code/Mod/Bots/BotBrain.cpp`, `m_tick < 0.1f`, строки 169–170).
- `BotLocomotion::Execute` проверяет предусловия и вызывает
  `BotMovement::Move`/`Stop`/`Aim`, `BotSuit::Apply`.
- `BotMovement` использует `CMovementRequest` и `IMovementController`,
  `BotCombat::Attack` — штатное оружие.

Сильные стороны: намерение действительно не мутирует физику; проверки
энергии/заземления/clearance находятся в исполнителе; отказ логируется.
Слабые места:

1. нет арбитра задач и приоритетов — порядок ветвлений зашит в `Update`;
2. нет структурированного результата: `bool` + строка в лог;
3. нет узкого интерфейса к миру → решения нельзя прогнать на фейковом мире;
4. нет стимулов (урон/звук) и оценки цели, кроме расстояния;
5. жёсткие пороги (18/22 м, 3 с памяти, 0.3/0.65 с стрельбы) не вынесены
   в профиль.

### 3.6 Рекомендации

**R1. Узкий интерфейс `IBotWorld` + host-тесты решений.** По образцу
`botlib_import_t` и тестового хоста YaPB
([`tests/host/fake_engine.h`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/tests/host/fake_engine.h),
[`tests/host/scenarios/test_combat.cpp`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/tests/host/scenarios/test_combat.cpp)).
Интерфейс: время, позиция/статы актёра, список врагов, LOS-луч, ближайшие
предметы, команды движения/оружия/костюма. Реальная реализация — тонкая
обёртка над `gEnv`; тестовая — детерминированная сцена.
*Модуль:* новый `Code/Mod/Bots/BotWorld.h` + `BotBrain`/`BotCombat`.
*Риски:* разрастание интерфейса; нельзя протаскивать `IEntity*` наружу.
*Стоимость:* 2–4 дня. *Приоритет:* высокий, это фундамент для R2–R3 и PR-1.

**R2. Арбитр намерений с приоритетом и возобновлением.** Взять `TaskPri`
и `TaskStack` YaPB, но без копирования кода: список `BotGoal` с
`priority`, `canInterrupt`, `resume`, временем жизни. Решение на 10 Гц:
собрать кандидатов (патруль, поиск оружия, бой, отступление, лечение),
выбрать максимум, сохранить вытесненную задачу.
*Модуль:* `BotBrain`/новый `BotDecision`.
*Риски:* дребезг целей, инверсия приоритетов, поломка текущих сценариев.
*Стоимость:* 1–2 дня после R1. *Приоритет:* высокий.

**R3. Структурированный результат действия.** Заменить `bool` + текст в логе
на `BotActionResult { Accepted, Blocked, NoEnergy, WrongMode, Cooldown,
NoClearance, NoWeapon, BadTarget }` с причиной. Образец — `bot_moveresult_t`
в Q3 ([`be_ai_move.c#L3055`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_ai_move.c#L3055))
и `StateStatus` Omni-bot.
*Модуль:* `BotLocomotion`, `BotMovement`, `BotSuit`, `BotCombat`, `BotWeapons`.
*Риски:* придётся сохранить старые подстроки логов, иначе сломается
`Scripts/Test-WarsBotsAbilityLogs.ps1`.
*Стоимость:* 1–2 дня. *Приоритет:* высокий (малый PR, без смены поведения).

## 4. Навигация: waypoint, AAS, navmesh, A*

### 4.1 Quake III: AAS и travel flags

AAS делит мир на **области**, между которыми строятся **reachability**:
`Jump`, `WalkOffLedge`, `Teleport`, `Elevator`, `JumpPad`, `Ladder`,
`WeaponJump` ([`be_aas_reach.c`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_aas_reach.c#L2109)).
Ключевая идея — **тип перехода известен заранее**, а не выясняется
эмпирически в момент движения. Стоимость перехода зависит от `travelflags`
(прыжок, вода, лестница, телепорт, запретные зоны).

Маршрутизация — не A* на каждый запрос, а **кластерные кэши**:
`AAS_GetAreaRoutingCache` ([`be_aas_route.c#L1399`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_aas_route.c#L1399))
и `AAS_AreaRouteToGoalArea` ([`#L1585`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_aas_route.c#L1585)).
Внутри кластера времена областей считаются один раз и переиспользуются;
между кластерами — портальный кэш. Публичный вход
`AAS_AreaTravelTimeToGoalArea` ([`#L1767`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_aas_route.c#L1767))
возвращает время; `AAS_AreaRouteToGoalArea` — время и номер достижимости.
Есть даже защита от роста кэша:
`while (AvailableMemory() < 1MB) AAS_FreeOldestCache()`.

`BotMoveToGoal` ([`be_ai_move.c#L3055`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_ai_move.c#L3055))
не пересчитывает путь, а идёт по reachability: выбирает следующую
достижимость с минимальным временем, обрабатывает blocked/avoid, отдельно
реагирует на jump pad и лифт. Это и есть «исполнение поверх заранее
размеченного графа».

**Вывод:** WarsBots нужен не «поиск пути по прямой», а граф с рёбрами,
у которых есть тип и предусловия. Прыжки и костюм — это не украшение,
а свойство ребра.

### 4.2 YaPB: A* со сменной эвристикой и флагами узлов

`AStarAlgo::Find` ([`planner.cpp#L488`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/planner.cpp#L488))
— обычный A* с очередью, метками Open/Closed, восстановлением пути и
защитой от циклов. Ценность в деталях:

- **сменные эвристики**: `PlannerHeuristicFast`, `Optimal`, `Safe`,
  `Diversity`; режим задаётся `cv_path_heuristic_mode` и переключает
  Chebyshev/Manhattan/диагональ/евклид/без эвристики
  ([`planner.cpp#L53-L61`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/planner.cpp#L53-L61));
- **стоимость ребра зависит от флагов узла**: `Crouch` и `Ladder` дают
  множитель 1.5, закрытые/опасные узлы — `kInfiniteHeuristic`;
- **сглаживание пути** `PostSmooth` и проверка `CantSkipNode`, чтобы
  срезание не проходило сквозь стену ([`#L462`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/planner.cpp#L462), [`#L415`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/planner.cpp#L415)).

Флаги узлов: `Button`, `Lift`, `Crouch`, `Ladder`, `DoubleJump`, `Narrow`,
`Sniper` ([`graph.h#L16-L33`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/inc/graph.h#L16-L33)).
Флаги рёбер: `PathFlag::Jump`. Это ровно та модель, которую можно
перенести на прыжки/спринт/костюм WarsBots.

### 4.3 RCBot2: waypoint A*, navmesh — заглушка

`CWaypointNavigator::workRoute` ([`bot_waypoint.cpp#L861`](https://github.com/APGRoboCop/rcbot2/blob/afa2d4e3f172f2e097036f1550b37230de48ecdf/utils/RCBot2_meta/bot_waypoint.cpp#L861))
строит маршрут по waypoint-графу с учётом условий и опасности, умеет
сохранять/загружать «belief» (незавершённый маршрут между жизнями).
Это подтверждает: в зрелом Source-боте основная навигация — waypoint, а не
navmesh.

Наглядная отрицательная находка: `CNavMeshNavigator` в
[`bot_navmesh.cpp#L41-L74`](https://github.com/APGRoboCop/rcbot2/blob/afa2d4e3f172f2e097036f1550b37230de48ecdf/utils/RCBot2_meta/bot_navmesh.cpp#L41-L74)
фактически заглушён: вызовы navmesh закомментированы, `workRoute` не
строит маршрут. Даже там, где navmesh движка доступен, он не стал
основным путём.

### 4.4 Omni-bot: два планировщика

- `PathPlannerFloodFill` + `Q3BSPFileImporter`/`VBSPFileImporter` — генерация
  графа из BSP-геометрии карты.
- `PathPlannerRecast` — Recast/Detour с тайлами и асинхронной пересборкой;
  в заголовке подключены `DetourTileCache`, `DetourTileCacheBuilder`,
  `DetourNavMesh`, `DetourNavMeshQuery`
  ([`PathPlannerRecast.h`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/PathPlannerRecast.h)).
  Сборка тайла заканчивается заполнением `dtNavMeshCreateParams`
  ([`PathPlannerRecast.cpp#L1778`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/PathPlannerRecast.cpp#L1778)),
  запросы идут через `findNearestPoly` и corridor-запросы.

Но `MovementCaps.h` содержит только `MOVE_WALK` и `MOVE_JUMP`. Специальные
переходы (лестницы, телепорты, jump pad, mover) живут не в caps, а в
nav-флагах и игровой логике вокруг планировщика. Для WarsBots это сигнал:
**типы переходов должны быть нашими, даже если геометрию считает Recast.**

### 4.5 Движок CryEngine 2 и фактическое состояние Steelmill

В SDK есть полноценный интерфейс графа:

- `IAISystem::GetNodeGraph()` ([`IAISystem.h#L866`](../Code/CryEngine/CryCommon/IAISystem.h#L866)),
- `IAISystem::LoadNavigationData(level, mission)` ([`#L817`](../Code/CryEngine/CryCommon/IAISystem.h#L817)),
- `IAISystem::IsSegmentValid(navCap, radius, from, to, navType)` ([`#L811`](../Code/CryEngine/CryCommon/IAISystem.h#L811)),
- `IGraph::GetEnclosing`, `GetNumNodeLinks`, `GetGraphLink`, `GetNextNode`,
  `GetNodePos`, `GetRadiusFromLink`, `GetNavType`
  ([`IAgent.h#L1825-L1880`](../Code/CryEngine/CryCommon/IAgent.h#L1825-L1880)),
- типы `NAV_TRIANGULAR` (MNM) и `NAV_WAYPOINT_HUMAN` ([`IAISystem.h#L628-L649`](../Code/CryEngine/CryCommon/IAISystem.h#L628-L649)).

То есть **теоретически** можно было бы не писать навигацию, а спросить граф
у движка. Проверка установленной игры показывает, почему это не работает:

- во всём `Crysis Wars` нет файлов `*.nav`, `*.mnm`, `*.navdata`;
- в `Game/Levels/multiplayer/ia/steelmill/level.pak` есть
  `areasmission0.bai` (36 байт: заголовок и нули),
  `roadnavmission0.bai` (8 байт), `waypt3dsfcmission0.bai` (8 байт),
  `netmission0.bai` (96 байт: заголовок и нулевые счётчики);
- следовательно, на Steelmill **нет ни waypoint-графа, ни готового MNM**,
  который можно читать через `IGraph`.

Crysis Co-op решает это так: на сервере в `OnLoadingStart` временно ставит
`gEnv->bMultiplayer = false`, вызывает `pAISystem->Init()`,
`FlushSystem()`, `Enable()`, `LoadNavigationData(...)`, затем возвращает
`bMultiplayer = true`; `ai_systemupdate` включается только на сервере
([`CoopSystem.cpp#L132-L160`](https://github.com/MikeCrossley/Crysis-Co-op/blob/138cf44e0fa134415e24ff19ff299e7198387733/Mods/CrysisCoop/Code/Coop/CoopSystem.cpp#L132-L160)).
В `ARCHITECTURE.md` решение 7 уже запрещает переносить это глобальное
переключение. Плюс `LoadNavigationData` загрузит пустые `.bai`, то есть
нам всё равно пришлось бы генерировать навигацию.

**Вывод:** движковый путь не даёт «бесплатной» навигации для Steelmill и
требует запрещённого глобального переключения. Его можно проверить только
как read-only спайк (R5), ожидаемый результат — ноль узлов.

### 4.6 Что предлагается WarsBots

**R4. Собственный waypoint-граф с типизированными рёбрами + A*.**
Формат: узлы (позиция, покрытие, радиус), рёбра (тип, стоимость, предусловие).
Типы рёбер: `Walk`, `Sprint`, `Crouch`, `Jump`, `StrengthJump`, `Drop`,
`Ladder` (позже), `Mover` (лифт/дверь), `Cloak` (позже).
Стоимость ребра — время + расход энергии; A* с эвристикой по расстоянию.
Исполнение остаётся за `BotLocomotion`: перед каждым ребром `Jump` —
`JumpSpace`, перед `Walk/Sprint` — `SafeStep`; при отказе ребро помечается
как временно недоступное и путь пересчитывается.
*Источники:* YaPB `AStarAlgo::Find` и `NodeFlag`/`PathFlag`; Q3 reachability
и travel flags; RCBot2 `workRoute`.
*Модуль:* новый `BotNavigation` + `BotIntent` (только точки/тип ребра) +
`BotLocomotion`.
*Данные:* сначала 20–30 узлов вручную/полуавтоматически на Steelmill,
затем генератор (raycast-сетка + валидация рёбер теми же `SafeStep`/`JumpSpace`).
*Риски:* нет данных на других картах; устаревание при смене карты; ложные
рёбра; прыжок Strength требует энергии и режима костюма.
*Стоимость:* 2–4 недели на полноценный v1 с генератором; 3–5 дней на
ручной граф из 20–30 узлов и A* без генератора.

**R5. Read-only спайк движкового графа (закрыть вопрос фактом).**
Команда `wb_nav_probe` в dev-сборке: `GetNodeGraph()`, `GetNumNodeLinks`
для нескольких точек, лог количества узлов; без `bMultiplayer`-переключения
и без `LoadNavigationData`. Ожидание: 0 узлов на Steelmill.
*Стоимость:* 0.5–1 день. *Риск:* минимальный. *Результат:* документированный
отказ от CryAI-навигации, чтобы не возвращаться к нему каждый месяц.

**R6. Heightmap-сетка как дешёвый fallback (не navmesh).**
Если граф окажется слишком трудоёмким: сетка 0.5–1 м по террейну
(`I3DEngine::GetTerrainElevation`, `GetTerrainSize`), проходимость — лучами
вниз/вперёд, A* по 8 соседям. Годится для открытых участков Steelmill,
но не для многоэтажных интерьеров. *Стоимость:* 3–5 дней.
*Риск:* 2048×2048 м при 0.5 м — 16.7 млн клеток (≈16 МБ на слой);
нужна тайловая обработка. *Вывод:* допустимо как временный слой, не как
основная навигация.

## 5. Восприятие, память, реакция, выбор оружия

### 5.1 Quake III

- `BotFindEnemy` ([`ai_dmq3.c#L2929`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/game/ai_dmq3.c#L2929))
  учитывает `enemysight_time`, видимость, расстояние, «не цель» и т.п.
- `BotAimAtEnemy` ([`#L3261`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/game/ai_dmq3.c#L3261))
  берёт `CHARACTERISTIC_AIM_SKILL`/`AIM_ACCURACY`; при очень высоком
  skill — ранний выход по `REACTIONTIME`; точность может зависеть от оружия.
- `BotCheckAttack` ([`#L3555`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/game/ai_dmq3.c#L3555))
  вводит `firethrottle`: бот не стреляет непрерывно, а чередует окна
  стрельбы/паузы; проверяет FOV и трассу.
- `BotChooseBestFightWeapon` ([`be_ai_weap.c#L404`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_ai_weap.c#L404))
  перебирает оружие и берёт максимум `FuzzyWeight` по инвентарю
  ([`be_ai_weight.c#L644`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_ai_weight.c#L644)).

### 5.2 YaPB: самая зрелая модель для подражания

- `Bot::SeesEnemy`/`TrackEnemies` + `EnemyMemorySpan`
  ([`combat.cpp#L375`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/combat.cpp#L375)):
  память зависит от skill, aggression и близости последнего контакта,
  ограничена 3–8 с. Это лучше фиксированных 3 с в WarsBots.
- `EngagementScore` ([`#L395`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/combat.cpp#L395)):
  `800/(distance+128)`, плюс бонусы за стрельбу, снайперку и низкое HP.
- `ShouldSwitchFocus` ([`#L426`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/combat.cpp#L426)):
  не переключаться без превышения 1.2× и не «налипать» втроём на одну цель
  (`TeammatesEngaging >= 2`).
- `GetAimBias` ([`#L887`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/combat.cpp#L887)):
  ошибка прицела зависит от оружия, дистанции, стойки, движения и
  «выставленности», масштабируется скиллом.
- `IsWeaponBadAtDistance`/`GetBestPrimaryCarriedIndex`
  ([`weapons.cpp#L698`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/weapons.cpp#L698),
  [`#L849`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/weapons.cpp#L849))
  и `NeedToPauseFiring` ([`#L324`](https://github.com/yapb/yapb/blob/7985904584a0755b216e17cc5de8c5fd28953895/src/weapons.cpp#L324))
  отделяют выбор оружия от собственно стрельбы.

### 5.3 RCBot2

`CBotVisibles` кэширует видимость по PVS и временным тикам:
`checkVisible`, `updateVisibles`, `isVisible`
([`bot_visibles.cpp#L181`](https://github.com/APGRoboCop/rcbot2/blob/afa2d4e3f172f2e097036f1550b37230de48ecdf/utils/RCBot2_meta/bot_visibles.cpp#L181),
[`#L247`](https://github.com/APGRoboCop/rcbot2/blob/afa2d4e3f172f2e097036f1550b37230de48ecdf/utils/RCBot2_meta/bot_visibles.cpp#L247),
[`#L390`](https://github.com/APGRoboCop/rcbot2/blob/afa2d4e3f172f2e097036f1550b37230de48ecdf/utils/RCBot2_meta/bot_visibles.cpp#L390)).
Это дешёвый способ не делать полный LOS для всех каждый тик.
Выбор врага идёт через фактор (`CFindEnemyFunc::execute`/`getFactor`),
а не «первый попавшийся».

### 5.4 Omni-bot

- `SensoryMemory::UpdateRecord` ([`BotSensoryMemory.cpp#L245`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/BotSensoryMemory.cpp#L245))
  обновляет запись; `GetMemoryRecord` ([`#L439`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/BotSensoryMemory.cpp#L439))
  создаёт/находит её. `MemoryRecord` хранит `TimeLastSensed`,
  `TimeBecameVisible`, `TimeLastVisible`, `IgnoreAsTarget`
  ([`MemoryRecord.h#L26-L60`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/MemoryRecord.h#L26-L60)).
- Стимулы: `UpdateWithSoundSource`, `UpdateWithTouchSource`
  ([`#L194-L217`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/BotSensoryMemory.cpp#L194-L217)).
- Выбор цели — **фильтры**: `FilterClosest`, `FilterMostHurt`,
  `FilterSensory`, `FilterBias`; targeting по умолчанию —
  `FilterClosest(...EntEnemy)` + `ENT_CAT_SHOOTABLE`
  ([`BotTargetingSystem.cpp#L56-L86`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/BotTargetingSystem.cpp#L56-L86)).
- Оружие — не «лучшее по таблице», а **запросы с приоритетом**:
  `AddWeaponRequest(Priority::ePriority, owner, weaponId)`
  ([`BotWeaponSystem.h#L75`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/BotWeaponSystem.h#L75)).

### 5.5 WarsBots сегодня

`BotCombat::FindEnemy` (10 Гц) держит текущую цель, пока она враг, жива и
в радиусе 80 м, и обновляет память при видимости; иначе ищет ближайшего
видимого врага (`BotCombat.cpp`). Память — 3 с (`now - m_lastSeen <= 3.0f`).
`BotCombat::Attack` проверяет LOS от глаз и от muzzle, направление оружия
(`Dot < 0.96`), патроны и перезарядку, затем стреляет очередями 0.2 с с
паузой 0.65 с. Ошибка прицела — детерминированная синусоида, до 0.25 м.

Чего нет: стимулов урона/звука, оценки угрозы, приоритета «добить раненого»,
учёта союзников, выбора оружия по дистанции, профилей реакции.

### 5.6 Рекомендации

**R7. Сенсорный снимок + стимулы.** Расширить `BotBlackboard`/`BotCombat`:
запись на каждого врага (последняя позиция, время видимости, время
последнего урона/звука), обновление видимости с кэшем (не каждый тик),
стимул `DamagedBy(EntityId, direction)` из `GameRules` при получении урона.
*Источники:* Omni `SensoryMemory`/`MemoryRecord`, RCBot2 `CBotVisibles`.
*Стоимость:* 3–5 дней. *Риски:* события урона в MP приходят на сервер —
проверить; не превратить в «чит-радар» (стимул только факт, не позиция
стрелка, если он не видим).
*Критерий:* бот, получивший урон сзади, разворачивается/ищет укрытие в
течение 0.5–1 с; память 3–8 с в зависимости от профиля.

**R8. Оценка цели и гистерезис переключения.** Взять формулу
`EngagementScore` + `ShouldSwitchFocus`: близость, стрельба, раненый,
снайпер; переключение только при 1.2× и с учётом занятости союзниками.
*Источники:* YaPB `combat.cpp`, RCBot2 `CFindEnemyFunc`, Omni filters.
*Стоимость:* 1–2 дня. *Риск:* дребезг и несправедливое «все на одного».
*Критерий:* сценарий с двумя врагами — ближайший стреляющий выбирается;
без превышения порога цель не меняется; лог фиксирует причину.

**R9. Политика выбора оружия отдельно от стрельбы.** Таблица
`weapon → {minRange, maxRange, dps, spread, ammoCost}`; выбор при смене
цели/дистанции/пустом магазине, но не в середине очереди. Перезарядка —
до смены, если есть патроны. *Источники:* Q3 `BotChooseBestFightWeapon`,
YaPB `IsWeaponBadAtDistance`, Omni weapon requests.
*Стоимость:* 2–3 дня. *Риски:* гонка перезарядки/смены, потеря SCAR
fallback. *Критерий:* 25 м — SCAR/FY71, 5 м — Shotgun/SMG; при 0 патронов —
перезарядка, затем смена; лог `WEAPON SELECT reason=range`.

## 6. Тактика, skill profiles, сложность

### 6.1 Что делают внешние проекты

- **Q3:** профиль — это набор `characteristic` (`AIM_SKILL`,
  `AIM_ACCURACY`, `REACTIONTIME`, `FIRETHROTTLE`, …), загружаемый из
  файла и интерполируемый по skill
  ([`be_ai_char.c#L496`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_ai_char.c#L496),
  [`#L516`](https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/botlib/be_ai_char.c#L516)).
  Сложность — число, а не ветка кода.
- **RCBot2:** `CBotProfile` хранит `m_iVisionTicks`, `m_iPathTicks`,
  `m_iSensitivity` (1–20, скорость «мыши»), `m_fBraveness` (0–1),
  `m_fAimSkill` (0–1) ([`bot_profile.h#L34-L41`](https://github.com/APGRoboCop/rcbot2/blob/afa2d4e3f172f2e097036f1550b37230de48ecdf/utils/RCBot2_meta/bot_profile.h#L34-L41)).
  Плюс utility-таблицы с `min/max` на класс.
- **YaPB:** `AimProfile` (оружие, дистанция, стойка, движение, exposure) и
  `AimFlags`; `Skill()` масштабирует ошибку; `agression_level_` влияет на
  память. Это уже не «три константы», а маленькая модель.
- **Omni-bot:** приоритеты оружия и фильтры цели; `TacticalManager`
  хранит/загружает **cover segments** и `Stimulus`
  ([`TacticalManager.cpp#L77-L115`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/TacticalManager.cpp#L77-L115)).

### 6.2 WarsBots сегодня

Пороги зашиты в `BotBrain::Update`: 18 м для начала атаки, 22 м для
удержания, 6/9 м для отступления, 3 с памяти, 0.3/0.65 с очередь/пауза.
Ошибка прицела — `min(0.25, distance*0.006)`. Один бот, один стиль.

### 6.3 Рекомендация

**R10. `BotProfile` как данные, значения по умолчанию = текущие.**
Поля: `reactionTime`, `aimSkill`, `aimAccuracy`, `fireThrottle`,
`memorySpan`, `engageRange`, `retreatRange`, `braveness`,
`visionTicks`, `pathTicks`, `sprintEnergyMin`, `cloakEnergyMin`.
Источники: RCBot2 `CBotProfile`, Q3 characteristics, YaPB `AimProfile`.
*Модуль:* новый `BotProfile.h` + `BotBrain`/`BotCombat`/`BotSuit`.
*Риски:* конфиг-зоопарк, недетерминизм тестов. Поэтому: фиксированный seed
на матч, значения по умолчанию повторяют текущие константы, профили — 3
пресета (`easy`, `normal`, `hard`), без внешних файлов в v1.
*Стоимость:* 3–5 дней. *Критерий:* два профиля дают измеримо разную
реакцию/ошибку при одинаковой сцене; `normal` побитово повторяет текущие
логи (кроме времени).

## 7. Server authority, синхронизация, масштабирование

### 7.1 Внешние проекты

- Q3 BotLib работает **на сервере** в game dll; клиенты получают обычное
  состояние игрока. Никакого клиентского AI.
- YaPB/RCBot2 — серверные плагины/модули; RCBot2 прямо throttles
  восприятие и путь через `m_iVisionTicks`/`m_iPathTicks` из профиля.
- Omni-bot — отдельная библиотека, но решения также на сервере;
  `UpdateDelay`/`Regulator` ограничивают частоту, `ConcurrentQueue`/
  `FiberManager` позволяют вынести тяжёлые запросы.
- Coop: `ai_systemupdate=1` только на сервере; клиенты — обычная
  репликация; `CDedicatedInput` в `AIDemoInput.h` показывает, как
  **штатный `IPlayerInput`** может генерировать движение и сериализовать
  input, но это отдельный экспериментальный путь, не обязательный для нас.

### 7.2 WarsBots

Сервер авторитетен: `BotSpawner::CanRunLocalIA()` — жёсткий gate;
`CPlayer::SetWarsBotMotion`/`SetWarsBotActions`/`PublishWarsBotJump`
работают только на сервере и только для своего актёра
(`Player.cpp#L4114-L4139`). Клиент получает `WarsBotMotion` в
`eEA_GameServerDynamic` внутри `BeginOptionalGroup`
(`Player.cpp#L4334-L4350`); клиент не создаёт физический прыжок, а
проигрывает анимацию по `jumpSerial` (L4163-L4169).

Решения — 10 Гц (`BotBrain.cpp#L169-L170`). На один тик сейчас:
`FindEnemy` (до N акторов, 2 LOS-луча на кандидата), `SafeStep`
(9 лучей + 1 вниз) на `Move`, при прыжке — `JumpSpace` (3 луча просвета +
landing + path + sphere sweep). При 8 ботах это порядка сотен–тысяч лучей/с — вероятно
приемлемо, но **не измерено**. Отдельная проблема —
`BotWeapons::FindNearby` перебирает **все** entity, когда у бота нет
подходящего оружия, но не чаще раза в 2 с (`BotWeapons.cpp#L54-L75`,
`BotBrain.cpp#L187-L195`); при большом числе сущностей это дороже, чем бой.

### 7.3 Рекомендации

**R11. Бюджет восприятия и round-robin.** Решения 10 Гц, но LOS/поиск
врагов — 2–5 Гц на бота, со сдвигом фазы; кэш видимости на N тиков;
`FindNearby` — только по actor/item system или через `GetEntitiesInBox`
в радиусе, а не по всем entity. *Источники:* RCBot2 `m_iVisionTicks`,
Omni `Regulator`/`UpdateDelay`.
*Стоимость:* 1–2 дня. *Критерий:* замер `ms` на тик при 1/2/4/8 ботах;
лимит лучей на тик; отсутствие роста времени от числа entity.

**R12. Не реплицировать путь и не добавлять обязательных полей.**
Клиентам нужны поза/цель/спринт/прыжок/костюм; узлы пути — только сервер.
Новые поля — только в `BeginOptionalGroup`, с совместимой версией DLL.
*Стоимость:* 0 (правило). *Риск:* нарушение ломает late join.

## 8. Прыжки, спринт и четыре режима нанокостюма

### 8.1 Как это устроено у внешних ботов

- Q3: прыжок/лестница/телепорт/jump pad — **типы reachability**;
  `WeaponJump`/`Grapple` — отдельные генераторы. Стоимость зависит от
  travel flags.
- YaPB: `PathFlag::Jump` на ребре, `NodeFlag::DoubleJump`, `Lift`,
  `Crouch`, `Narrow`. То есть возможность — свойство ребра, а не
  глобальное состояние.
- Omni-bot: off-mesh connections Recast + nav-флаги
  `NAVFLAGS_JUMP|LADDER|TELEPORT|DOOR|ROCKETJUMP|MOVER|JUMPPAD`
  (таблица флагов `jump/ladder/teleport/door/rocketjump/mover/jumppad` —
  [`PathPlannerBase.cpp#L28-L38`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/PathPlannerBase.cpp#L28-L38)).
  Но `MovementCaps` — только WALK/JUMP, остальное живёт в игровой логике.

### 8.2 Что уже есть в WarsBots (сильная сторона)

`BotIntent` уже различает `Walk/Run/Sprint`, `Normal/Strength` jump,
`STANCE_*` и `ENanoMode`. `BotLocomotion::JumpSpace` **эмпирически**
проверяет просвет, площадку, уклон и коллизию капсулы
(`BotLocomotion.cpp#L63-L85`), а `Execute` — энергию, режим, cooldown,
заземление, prone (`#L86-L123`). `BotSuit::Apply` — пороги энергии и
гистерезис 2 с (`BotSuit.cpp#L12-L40`). Это лучше, чем слепо верить
разметке: граф говорит «здесь можно», исполнитель всё равно проверяет.

### 8.3 Рекомендация

**R13. Возможности — на рёбрах, проверка — в исполнителе.**
В графе:
`requires = { gait, suitMode, minEnergy, stance }`,
`cost = baseTime + energyCost`.
Прыжок Strength — отдельный тип ребра (дальность до ~3.6 м подъёма по
замерам, но для планирования консервативно 1.5 м по `JumpSpace`).
Спринт — только на рёбрах длиннее N метров и при энергии > порога;
Cloak — отдельный тип ребра с ценой энергии и запретом стрельбы.
Defense — не ребро, а приоритетный сброс: при уроне/стрельбе режим Defense
и отмена спринта (как уже делает `BotBrain::ExecuteIntent`). Перед
выполнением ребра — `JumpSpace`/`SafeStep`; при отказе — пометить
ребро недоступным на T секунд и перепланировать.
*Источники:* Q3 travel flags/reachability, YaPB `NodeFlag`/`PathFlag`.
*Риски:* граф обещает больше, чем физика; энергия закончилась в середине
маршрута — нужен fallback на Walk.
*Стоимость:* входит в R4 (плюс 2–4 дня на типы рёбер и валидацию).
*Критерий:* маршрут с одним Strength-jump и одним спринт-участком
проходится без ручных команд; при энергии ниже порога выбирается
Walk-маршрут; ни одного «телепорта»/импульса.

## 9. Лицензии и совместимость с Crytek MOD SDK

| Проект | Лицензия | Перенос кода | Комментарий |
| --- | --- | --- | --- |
| Q3 BotLib | **GPL-2.0** | **нет** | GPL несовместим с закрытым CryEngine/MOD SDK и с закрытой поставкой DLL; идеи и алгоритмы — можно |
| YaPB | **Unlicense** (public domain) | технически да | но код завязан на GoldSrc (`edict_t`, `pev`); копирование принесёт чужую модель мира; лучше переписывать |
| RCBot2 | **AGPL-3.0** (репозиторий; в старых файлах — GPLv2 с исключением) | **нет** | сетевой copyleft: раскрытие исходников пользователям сервера; категорически не линковать |
| Omni-bot | корневой лицензии **нет** (SPDX 404), часть `GameInterfaces/ET-GPL` — GPL, bundled curl/boost/protobuf и др. | **нет** | смешанные/неясные права; считать «all rights reserved» |
| Recast/Detour | **Zlib** | да, при сохранении notice | совместим со статической линковкой в mod; не copyleft |
| Crysis Co-op | лицензии **нет**, производная Crytek MOD SDK | **нет** | только изучение; код и ассеты не переносить |
| Crytek MOD SDK | закрытая лицензия | — | наш DLL остаётся модом; нельзя перелицензировать в GPL/AGPL; сохранять `LICENSE.txt` Crytek |

Практический вывод: **единственная зависимость, которую в принципе можно
добавить, — Recast/Detour (Zlib)**, и только после спайка. Всё остальное —
источник идей и тестовых сценариев, не код.

## 10. Recast/Detour для Steelmill: детальный разбор

### 10.1 Что именно требует Recast

Recast — не «библиотека навигации», а **компилятор треугольной геометрии
в navmesh**. Вход: массив вершин и индексов (triangle soup), границы мира и
`rcConfig` ([`Recast.h#L216-L276`](https://github.com/recastnavigation/recastnavigation/blob/9f4ce64458dfae86e1239c525ddc219c4e9e06f1/Recast/Include/Recast.h#L216-L276)).
Пайплайн: `rcRasterizeTriangles` → `rcBuildCompactHeightfield` →
`rcBuildContours` → `rcBuildPolyMesh` → `rcBuildPolyMeshDetail`.
`rcConfig` задаёт `cs`, `ch`, `walkableSlopeAngle`, `walkableHeight`,
`walkableClimb`, `walkableRadius`, `maxEdgeLen`, `maxSimplificationError`,
`minRegionArea`, `mergeRegionArea`, `detailSampleDist`.

Detour — runtime: `dtNavMesh` (тайлы), `dtNavMeshQuery::findPath`
([`DetourNavMeshQuery.cpp#L973`](https://github.com/recastnavigation/recastnavigation/blob/9f4ce64458dfae86e1239c525ddc219c4e9e06f1/Detour/Source/DetourNavMeshQuery.cpp#L973)),
`findNearestPoly` ([`#L693`](https://github.com/recastnavigation/recastnavigation/blob/9f4ce64458dfae86e1239c525ddc219c4e9e06f1/Detour/Source/DetourNavMeshQuery.cpp#L693)),
`dtPathCorridor::movePosition`
([`DetourPathCorridor.cpp#L439`](https://github.com/recastnavigation/recastnavigation/blob/9f4ce64458dfae86e1239c525ddc219c4e9e06f1/DetourCrowd/Source/DetourPathCorridor.cpp#L439)).
Off-mesh связи — `dtOffMeshConnection` и
`getOffMeshConnectionPolyEndPoints`
([`DetourNavMesh.h#L224`](https://github.com/recastnavigation/recastnavigation/blob/9f4ce64458dfae86e1239c525ddc219c4e9e06f1/Detour/Include/DetourNavMesh.h#L224),
[`#L459`](https://github.com/recastnavigation/recastnavigation/blob/9f4ce64458dfae86e1239c525ddc219c4e9e06f1/Detour/Include/DetourNavMesh.h#L459)).

### 10.2 Геометрия Steelmill: что доступно и чего не хватает

| Источник | API в нашем SDK | Пригодность |
| --- | --- | --- |
| Террейн | `I3DEngine::GetTerrainElevation` ([`I3DEngine.h#L1513`](../Code/CryEngine/CryCommon/I3DEngine.h#L1513)), `GetTerrainSize` ([`#L1552`](../Code/CryEngine/CryCommon/I3DEngine.h#L1552)), `GetHeightMapUnitSize` ([`#L1542`](../Code/CryEngine/CryCommon/I3DEngine.h#L1542)); `terrain/heightmap.dat` 2.8 МБ в `level.pak` | **да**, но это heightfield; для Recast нужно либо триангулировать сетку, либо использовать `rcRasterizeTriangles` по сэмплам |
| Физический heightfield | `IPhysicalWorld::GetHeightfieldData(primitives::heightfield*)` ([`physinterface.h#L2171`](../Code/CryEngine/CryCommon/physinterface.h#L2171)); `primitives::heightfield` ([`primitives.h#L48`](../Code/CryEngine/CryCommon/primitives.h#L48)) | **да** для земли; holes/материалы доступны |
| Статические объекты | `IStatObj::GetIndexedMesh` ([`IStatObj.h#L144`](../Code/CryEngine/CryCommon/IStatObj.h#L144)), `GetPhysGeom` ([`#L159`](../Code/CryEngine/CryCommon/IStatObj.h#L159)); `I3DEngine::GetLoadedStatObjArray` ([`I3DEngine.h#L1248`](../Code/CryEngine/CryCommon/I3DEngine.h#L1248)) | **частично**: массив даёт шаблоны объектов, **но не мировые трансформации экземпляров** |
| Экземпляры через физику | `GetEntitiesInBox` ([`physinterface.h#L2236`](../Code/CryEngine/CryCommon/physinterface.h#L2236)); `pe_status_pos::pos/q/scale/pGeom` ([`#L934-L955`](../Code/CryEngine/CryCommon/physinterface.h#L934-L955)); `IGeometry::GetPrimitive/GetData`; `mesh_data` ([`#L1547`](../Code/CryEngine/CryCommon/physinterface.h#L1547)) с `pVertices`/`pIndices`/`pIslands` | **да, но с работой**: обойти мир боксами, отфильтровать `ent_static`, собрать треугольники с трансформом |
| BSP/`.cry` | level.pak содержит `brush/*.cgf` и `brush.lst`; сама карта — `Steelmill.cry` | **нет готового BSP-парсера**; писать свой парсер `.cry`/CGF — большой отдельный проект |

Ключевой риск: в CryEngine статическая геометрия может быть **слита** в
крупные физические объекты/areas. Тогда `GetEntitiesInBox` вернёт не
отдельные brush'и, а большие меши; Recast это переварит, но нужно
проверить размеры и число треугольников. Без эмпирического спайка
утверждать «геометрию получить легко» нельзя.

### 10.3 Построение: параметры и стоимость

Для спайка разумны консервативные значения (не истина, а стартовая точка):

```
cs = 0.3      // 30 см на клетку
ch = 0.2
walkableSlopeAngle = 45
walkableHeight = 9   // 1.8 м
walkableClimb = 2    // 0.4 м
walkableRadius = 2   // радиус 0.6 м; агент ~0.35 м
maxEdgeLen = 40      // 12 м
maxSimplificationError = 1.3
minRegionArea = 64
mergeRegionArea = 400
detailSampleDist = 6.0
```

При террейне 2048×2048 м и `cs=0.3` это ~6.8k×6.8k клеток в слое, то есть
**обязательные тайлы** (Omni-bot использует `TileSize=128`,
[`PathPlannerRecast.cpp#L239`](https://github.com/jswigart/omni-bot/blob/c2f664a9b03202e78c5260ac0924b467dff558d3/Omnibot/Common/PathPlannerRecast.cpp#L239)).
Без тайлов память и время сборки неприемлемы. Тайлы нужны и для динамики
(двери/лифты), и для стриминга.

Оценка (требует замера): полный offline-bake Steelmill — от десятков секунд до минут
(зависит от числа треугольников и `cs`); runtime-генерация при загрузке
карты — риск фриза на сервере. Поэтому bake должен быть **отдельным
инструментом/консольной командой dev-сборки**, а результат — файлом в моде,
а не работой при старте матча.

### 10.4 Вертикальные переходы

Recast сам по себе **не выводит прыжки из геометрии**. `walkableClimb`
покрывает только ступени/уклоны. Всё остальное — `dtOffMeshConnection`,
которые кто-то должен создать:

- прыжок через пропасть/перила — двунаправленная off-mesh связь;
- Strength jump — отдельный тип связи с предусловием режима/энергии;
- лестница — последовательность связей или отдельный volume;
- лифт/дверь — динамическая связь, включаемая по состоянию объекта
  (`Mover`), с пересборкой тайла;
- телепорт/jump pad — связь с триггером.

Omni-bot именно так и делает: nav-флаги + отдельные связи, а не «магия
Recast». Значит, даже с Recast нам нужен **свой слой разметки переходов** —
тот же, что и для waypoint-графа (R4/R13). Это главный аргумент против
«Recast решит навигацию».

### 10.5 Подключение к текущему `BotMovement`

Корректная схема, сохраняющая инварианты:

```
BotNavigation::Request(start, goal, capabilities) -> waypoints[]
  (Detour findPath + straight path; off-mesh links -> waypoint с типом)
BotBrain (10 Гц): взять следующий waypoint -> BotIntent.MoveTo(...)
BotLocomotion: SafeStep / JumpSpace / энергия / режим
  отказ -> BotNavigation::Invalidate(edge, T) -> перепланирование
```

Detour-коридор `movePosition` нужен только если мы хотим непрерывное
локальное обновление; при 10 Гц достаточно перезапроса пути/следующей
точки. `dtCrowd` для одного бота не нужен и вреден: он делает собственное
локальное avoidance, которое будет конфликтовать с `SafeStep` и физикой
движка. `BotMovement` остаётся единственным местом, где выдаётся
`CMovementRequest`.

### 10.6 Стоимость и риски

| Работа | Оценка |
| --- | --- |
| Вендоринг Recast/Detour + CMake + zlib notice | 1–2 дня |
| Извлечение треугольников (террейн + static + transforms) | 1–2 недели, риск высокий |
| Offline-bake tool + формат/версия/загрузка | 1–2 недели |
| Тайлы, память, время сборки, стриминг | 1–2 недели |
| Off-mesh переходы (jump/strength/lift) + валидация | 1–2 недели |
| Интеграция с BotNavigation/BotLocomotion + отладка | 1 неделя |
| **Итого** | **6–9 недель** при удачном извлечении геометрии |

Риски: (1) геометрию не удастся извлечь полно и корректно; (2) bake
не влезет в память без тайлов; (3) прыжки всё равно придётся размечать
вручную; (4) ассеты navmesh нужно версионировать и поставлять с модом;
(5) детерминизм и совместимость версий; (6) zlib-код нужно сопровождать.

### 10.7 Вердикт

**Recast/Detour не объявляется подходящим решением на этом этапе.**
Библиотека качественная и лицензионно совместимая, но её ценность
появится только после того, как доказаны три вещи:

1. геометрия Steelmill извлекается полностью и за приемлемое время;
2. bake даёт проходимый navmesh без дыр на ключевых маршрутах;
3. jump/off-mesh связи можно генерировать и валидировать дешевле, чем
   вручную.

Пока эти три пункта не доказаны, **waypoint-граф с типизированными
рёбрами (R4) дешевле, прозрачнее и лучше использует уже написанные
`SafeStep`/`JumpSpace`**. Recast-спайк (R14) имеет смысл только как
отдельная ветка/инструмент с критериями ниже и **без мержа в игровую
логику** до прохождения gate.

**R14. Recast-спайк (gate, не мержить в gameplay).**
Критерии приёмки спайка:

1. Извлечены треугольники всей Steelmill; в лог — число треугольников,
   вершин, время и пиковая память; нет пропущенных крупных зон (сверить
   по 10 контрольным точкам).
2. Собран navmesh с параметрами §10.3 в тайлах; в лог — число тайлов,
   полигонов, время bake, размер файла.
3. `findPath` находит путь между 5 парами точек (спавн ↔ центр ↔ верхний
   уровень ↔ дальняя точка); путь визуализирован/залогирован.
4. 10 off-mesh связей (обычный и Strength jump) созданы и проверены:
   `JumpSpace` на сервере подтверждает каждую.
5. Ни одного краша/зависания; bake не выполняется при старте матча.
6. Если любой пункт не выполнен — спайк закрывается, остаётся R4.

*Стоимость спайка:* 1–2 недели. *Решение о внедрении:* только после
отдельного согласования.

## 11. Матрица рекомендаций

| ID | Что | Модуль WarsBots | Источник | Стоимость | Риск | Приоритет |
| --- | --- | --- | --- | --- | --- | --- |
| R1 | `IBotWorld` + host-тесты | `BotBrain`/`BotCombat`/новый `BotWorld` | Q3 `botlib_import_t`, YaPB `tests/host` | 2–4 дня | разрастание интерфейса | **1** |
| R2 | арбитр намерений | `BotBrain` | YaPB `TaskStack`/`TaskPri` | 1–2 дня | дребезг целей | **1** |
| R3 | структурированный результат | `BotLocomotion`/`BotSuit`/`BotCombat` | Q3 `bot_moveresult_t` | 1–2 дня | сломать лог-тесты | **1** |
| R4 | waypoint-граф + A* | новый `BotNavigation` | YaPB `AStarAlgo`/`NodeFlag` | 3–5 дней (ручной) / 2–4 нед. (генератор) | нет данных, устаревание | **2** |
| R5 | read-only спайк CryAI-графа | dev-команда | `IAISystem`/`IGraph` | 0.5–1 день | минимальный | 2 |
| R6 | heightmap-сетка (fallback) | `BotNavigation` | Recast heightfield, движковый террейн | 3–5 дней | 16 МБ/слой, интерьеры | 3 |
| R7 | сенсорная память + стимулы | `BotBlackboard`/`BotCombat` | Omni `SensoryMemory`, RCBot2 `CBotVisibles` | 3–5 дней | события урона в MP | **2** |
| R8 | оценка цели + гистерезис | `BotCombat` | YaPB `EngagementScore`/`ShouldSwitchFocus` | 1–2 дня | «все на одного» | **2** |
| R9 | политика выбора оружия | `BotWeapons`/`BotCombat` | Q3 `FuzzyWeight`, YaPB `IsWeaponBadAtDistance` | 2–3 дня | гонка перезарядки | 2 |
| R10 | профили skill/сложности | новый `BotProfile` | RCBot2 `CBotProfile`, Q3 characteristics | 3–5 дней | недетерминизм | 3 |
| R11 | бюджет восприятия | `BotBrain`/`BotSpawner` | RCBot2 ticks, Omni `Regulator` | 1–2 дня | устаревание данных | 3 |
| R12 | не реплицировать путь | `Player.cpp` (правило) | — | 0 | нарушение optional group | — |
| R13 | типы рёбер для прыжков/костюма | `BotNavigation`/`BotLocomotion` | Q3 travel flags, YaPB `PathFlag` | +2–4 дня к R4 | граф обещает лишнее | **2** |
| R14 | Recast-спайк (gate) | отдельная ветка/инструмент | Recast/Detour | 1–2 нед. спайк; 6–9 нед. внедрение | геометрия/память/связи | 4 |

Приоритет 1 — делать первым; 2 — после стабилизации текущей фазы; 3 —
по мере роста; 4 — только по отдельному решению.

## 12. Что не делать

- **Не переносить код Q3/RCBot2/Omni-bot/Coop.** Лицензии (GPL/AGPL/нет
  лицензии) несовместимы с закрытым MOD SDK; копирование создаст
  юридический и сопроводительный долг.
- **Не включать CryAI через `bMultiplayer=false`.** Это уже запрещено
  `ARCHITECTURE.md`; на Steelmill нет навигационных данных.
- **Не делать navmesh «потому что Recast хороший».** Без геометрии,
  off-mesh связей и bake-пайплайна это не работает.
- **Не добавлять `dtCrowd` и собственное avoidance** поверх
  `CMovementRequest`/`SafeStep` — два контроллера начнут конфликтовать.
- **Не реплицировать путь и не расширять обязательные сетевые поля.**
- **Не заменять `SafeStep`/`JumpSpace` доверием к графу.** Граф —
  подсказка, физика — истина.
- **Не вводить GOAP** как «современную архитектуру»: в открытом Omni-bot
  это заглушка, а в Q3/YaPB/RCBot2 работают state machine/задачи/utility.

## 13. План малых независимых PR

Все PR — небольшие, с проверяемым результатом; spike-PR не мержатся в
игровую логику.

**PR-1. `IBotWorld` + host-тест решений (R1).**
*Содержание:* интерфейс, реальная обёртка, фейковый мир, 3 сценария
(видит врага / не видит / нет патронов). *Критерий:* тесты запускаются
локально без игры; поведение бота в игре не меняется (сравнение логов
`start/stop/pickup`); новый код не тянет `IEntity*` в решения.
*Оценка:* 2–4 дня.

**PR-2. `BotActionResult` (R3).**
*Содержание:* enum + причина, замена `bool` в `Move`/`Execute`/`Apply`/
`Attack`; старые строки логов сохранены. *Критерий:*
`Scripts/Test-WarsBotsAbilityLogs.ps1` проходит без правок; каждый отказ
имеет код; ни одного `ACTION REFUSED: ` без кода.
*Оценка:* 1–2 дня.

**PR-3. Арбитр намерений (R2).**
*Содержание:* `BotGoal` (id, priority, canInterrupt, resume), выбор на
10 Гц, сохранение вытесненной задачи. *Критерий:* сценарий «патруль →
враг → потеря врага → возврат к патрулю/оружию»; нет двойного `StartFire`;
`wb_bot_stop` сбрасывает стек; поведение при смерти/respawn не меняется.
*Оценка:* 1–2 дня.

**PR-4. Оценка цели и гистерезис (R8).**
*Содержание:* `ThreatScore`, `ShouldSwitchTarget`, порог 1.2×, лог
причины. *Критерий:* сцена с двумя врагами (ближний стреляет, дальний
ранен) — выбран ближний; без 1.2× цель не меняется; нет переключения
чаще, чем раз в 1 с.
*Оценка:* 1–2 дня.

**PR-5. Политика оружия (R9).**
*Содержание:* таблица диапазонов, выбор перед очередью, перезарядка
раньше смены. *Критерий:* 25 м — SCAR/FY71; 5 м — Shotgun/SMG; 0 патронов
— reload → switch; в логе `WEAPON SELECT reason=...`; SCAR fallback
сохраняется.
*Оценка:* 2–3 дня.

**PR-6. Профили (R10).**
*Содержание:* `BotProfile` + 3 пресета, `normal` = текущие значения.
*Критерий:* при одинаковой сцене `hard` реагирует быстрее и точнее
`easy` (метрика в логе); `normal` не меняет существующие проверки;
seed фиксирован.
*Оценка:* 3–5 дней.

**PR-7. Waypoint-граф v0 + A* (R4, R13).**
*Содержание:* формат, загрузка, A*, 20–30 узлов Steelmill, типы рёбер
`Walk`/`Sprint`/`Jump`/`StrengthJump`. *Критерий:* 3 маршрута (спавн →
центр, центр → верхний уровень, дальняя точка → спавн) проходятся без ручных
команд; каждое jump-ребро подтверждено `JumpSpace`; при отказе — переплан
и запись `EDGE INVALID`; нет застревания > 5 с.
*Оценка:* 3–5 дней (ручной граф) + 2–4 недели (генератор, отдельный PR).

**PR-8. Read-only CryAI probe (R5), не мержить в gameplay.**
*Содержание:* dev-команда, лог `GetNodeGraph()`/числа узлов на Steelmill.
*Критерий:* на Steelmill 0 узлов; отсутствие крашей и изменений
`bMultiplayer`; результат зафиксирован в `STATUS`/этом документе.
*Оценка:* 0.5–1 день.

**PR-9. Recast bake spike (R14), отдельная ветка.**
*Содержание:* вендоринг Recast/Detour, извлечение геометрии, bake,
`findPath`, 10 off-mesh. *Критерий:* §10.7, пункты 1–6. *Решение о
мерже:* только после отдельного согласования; до этого — не игровой код.
*Оценка:* 1–2 недели.

## 14. Итоговые три решения

1. **R1 + R3: сделать решения тестируемыми и наблюдаемыми.**
   Узкий `IBotWorld` и структурированный `BotActionResult` не меняют
   поведение, но дают возможность проверять логику без игры и точно
   понимать, почему действие отклонено. Это самый дешёвый шаг с
   наибольшим долгосрочным эффектом; он также защищает инвариант
   «решение не мутирует физику».

2. **R4 + R13: собственный waypoint-граф с типизированными рёбрами.**
   Steelmill не имеет пригодной движковой навигации (пустые `.bai`,
   запрещённое переключение `bMultiplayer`), а Recast требует
   геометрии/off-mesh/пайплайна. Граф с рёбрами `Walk/Sprint/Jump/
   StrengthJump` использует уже проверенные `SafeStep`/`JumpSpace`,
   даёт маршруты и позволяет добавлять костюм как свойство ребра.
   Это разблокирует «настоящую» навигацию без 6–9 недель Recast.

3. **R7 + R8 + R10: восприятие, оценка цели и профиль.**
   Сенсорная память со стимулами, `EngagementScore` с гистерезисом и
   профили сложности дают заметно более «живого» бота за 5–10 дней и
   не требуют ни одной внешней зависимости. Это следующий по ценности
   слой после стабилизации текущей фазы способностей.

## 15. Открытые вопросы (что измерить, а не предполагать)

1. Сколько статических физических объектов и треугольников на Steelmill?
   (`GetEntitiesInBox` + `pe_status_pos.pGeom`.)
2. Сколько времени занимает bake при `cs=0.3` и `TileSize=128`?
3. Приходят ли на сервер события урона по боту в MP (для R7) и в каком
   виде?
4. Сколько лучей/мс тратит текущий `SafeStep` при 1/2/4/8 ботах?
5. Есть ли на других картах IA/PS непустые `.bai`? (Проверить все
   `level.pak` перед тем, как хоронить CryAI окончательно.)
6. Достаточно ли 20–30 узлов для Steelmill или нужен генератор?
