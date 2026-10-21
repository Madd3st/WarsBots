# Исследование SDK и виртуального игрока

Дата: 2026-10-21. Ниже разделены подтверждённые свойства открытого кода и гипотезы,
для которых необходим работающий dedicated server с реальным клиентом.

## Этап 2: сравнение подходов и выбранный эксперимент

| Подход | Доказательства из SDK | Оценка |
| --- | --- | --- |
| A: серверный актёр без клиента | IActorSystem::CreateActor; CActor::Init вызывает BindToNetwork; GetSpawnInfo/SerializeSpawnInfo передают teamId; CPlayer::InitClient и CActor::InitClient обслуживают подключившийся клиент | Достаточно для **эксперимента создания**, недостаточно для утверждения, что это сетевой участник IA/PS |
| B: виртуальный сетевой клиент | INetwork::CreateNub, INetNub::ConnectTo и IGameNub::CreateChannel существуют, но CreateChannel получает настоящий INetChannel; CryAction управляет игровыми каналами/контекстом | Готового AddFakeClient/CreateVirtualChannel нет в изученных интерфейсах. Потребовалась бы реализация совместимого endpoint, handshake, context establishment и input либо настоящий отдельный клиент. Не выбран |

Ключевые дополнительные файлы: `Code/CryEngine/CryCommon/INetwork.h` (CreateNub,
IGameNub, INetNub), `Code/CryEngine/CryAction/IGameObjectSystem.h` (extension factory,
CreateGameObjectForEntity, SetSpawnSerializer), `IEntitySystem.h` (SpawnEntity,
RemoveEntity, sink), `Actor.cpp::GetSpawnInfo/SerializeSpawnInfo`, `Player.cpp::InitClient`.
IGameObjectSystem не выдаёт сетевых каналов; создание голой EntitySystem-сущности
не заменяет actor extension/inventory/network binding. Поэтому probe вызывает
CreateActor напрямую, не SpawnPlayer с поиском существующего актёра по общему нулю.

В эксперименте A штатный CActor::Init выполняет network binding ровно один раз.
Собственный RMI-протокол, ручная BindToNetwork и модификация NetSerialize не добавлены.
Spawn info и InitClient указывают на предусмотренный механизм подключения новых
клиентов, но закрытая реализация CryAction/CryNetwork не позволяет доказать late join
только чтением заголовков. Этот результат требует NETWORK_TESTS.

BotSpawner хранит только собственный EntityId и pointer identity для сравнения
(не разыменовывает сохранённый указатель). ENTITY_EVENT_DONE сбрасывает владение;
удаление разрешается только для собственной сущности. OnLoadingStart/Error,
OnLevelEnd, resetBegin, disconnect и Shutdown выполняют очистку. ILevelSystemListener
в этой версии SDK **не имеет OnUnload**: поэтому дополнительно используются
IGameFrameworkListener и IEntitySystemSink. Sink никогда не отменяет чужой spawn/remove.

Ни IsPlayer, ни GetChannelId, ни GameRules/Player/Actor не изменены. Оставшиеся задачи
для IA: отдельный participant registry по EntityId, объединённое перечисление людей
и ботов, безопасные bot-aware IsPlayer-проверки на уровне мода, kill credit/scoreboard,
спавн и respawn через исследованные Lua, shot validation и player-count policy.
Для PS дополнительно нужны team maps без фиктивного channel, безопасные objective/
voice ветки, entity-based Prestige Points и разрешённые сервером respawn/purchase/
capture callbacks. Экономика, навигация, бой и транспорт сейчас не реализуются.

## Источники и лицензии

| Репозиторий | Изученная ревизия | Назначение |
| --- | --- | --- |
| [cw-mod-sdk](https://github.com/ccomrade/cw-mod-sdk/tree/98c3dad11af50c298a5564a4b50c053a5ca11bf6) | `98c3dad11af50c298a5564a4b50c053a5ca11bf6` | База, современный CMake/MSVC/C++20, Code/CryEngine — интерфейсы, Code/Mod — игровая DLL. |
| [Crysis-Co-op](https://github.com/MikeCrossley/Crysis-Co-op/tree/138cf44e0fa134415e24ff19ff299e7198387733) | `138cf44e0fa134415e24ff19ff299e7198387733` | README, дерево, CoopSystem.cpp; Code/CryEngine и Mods/CrysisCoop/Code, Lua и другие mod assets. |
| [Crysis-Wars-Source-Code](https://github.com/AStopher/Crysis-Wars-Source-Code/tree/9cfe9fa887f6583b72f3bf1dc3c5609077e44fc6) | `9cfe9fa887f6583b72f3bf1dc3c5609077e44fc6` | README, дерево, контроль реализации SpawnPlayer; старый VS solution в Mods/CrysisWarsMod/Code. |

У базы сохранён [LICENSE.txt](../LICENSE.txt): Crytek Limited License Agreement,
не MIT/BSD/GPL. В частности, раздел 2 ограничивает использование модификациями
полных Warhead/Wars и описывает бесплатное электронное распространение вместе с
соглашением; раздел 5 содержит дополнительные некоммерческие ограничения и
сохранение notices. Это описание текста поставки, не заключение о правовом статусе
всего будущего проекта. Лицензию не заменяли. README upstream предлагает trial,
но для этого проекта инструкция ориентируется на полную игру согласно LICENSE.

В полных деревьях двух референсов не найден отдельный LICENSE/COPYING.
README AStopher приглашает создавать моды, но этого недостаточно для приписывания
репозиторию свободной лицензии или прав на весь Crytek-код. Co-op не даёт в README
явного разрешения на перенос реализации. Оба использованы только как источники
анализа; код, скрипты и бинарные материалы из них не импортировались.

## Создание актёра и идентичность

[GameFactory.cpp](https://github.com/ccomrade/cw-mod-sdk/blob/98c3dad11af50c298a5564a4b50c053a5ca11bf6/Code/Mod/GameFactory.cpp#L131)
регистрирует `Player` как CPlayer. IA/TIA/PS используют CGameRules и aliases ia/tia/ps.
[GameDll.cpp](../Code/Mod/GameDll.cpp) экспортирует CreateGame и инициализирует окружение
модуля перед placement-new CGame. Именно CGame, а не DllMain, подходит для bootstrap.

[GameRules.cpp:861](https://github.com/ccomrade/cw-mod-sdk/blob/98c3dad11af50c298a5564a4b50c053a5ca11bf6/Code/Mod/GameRules.cpp#L861):
SpawnPlayer проверяет bServer, ищет актёра по channelId и при отсутствии вызывает
IActorSystem::CreateActor. Он **не** добавляет игрока в m_channelIds, не создаёт
соединение, не вызывает OnClientEnteredGame и не запускает bot input.
Контроль по старому [GameRules.cpp AStopher](https://github.com/AStopher/Crysis-Wars-Source-Code/blob/9cfe9fa887f6583b72f3bf1dc3c5609077e44fc6/Mods/CrysisWarsMod/Code/GameRules.cpp)
показывает ту же последовательность, а не скрытую реализацию fake client.

[IActorSystem.h:122,162](https://github.com/ccomrade/cw-mod-sdk/blob/98c3dad11af50c298a5564a4b50c053a5ca11bf6/Code/CryEngine/CryAction/IActorSystem.h#L122):
`IsPlayer()` — невиртуальная проверка GetChannelId()!=0; CreateActor принимает
uint16 channelId и необязательный EntityId. EntityId идентифицирует сущность,
channelId связывает GameObject с сетевым игроком. Это разные пространства имён.
Сам CreateActor реализован в закрытом CryAction, здесь доступен только интерфейс.
Поэтому возможность успешного создания/репликации без соединения статически не доказана.

[Actor.cpp:334](../Code/Mod/Actor.cpp) создаёт movement controller, регистрирует актёра
через AddActor, подключает script bind, AnimatedCharacter и Inventory, вызывает
BindToNetwork. Неудача binding возвращает false. CActor::SetChannelId в этом файле
пуст; его не следует принимать за механизм создания виртуального сетевого игрока.

## Регистрация в матче: почему фиктивный канал опасен

[GameRules.cpp:549](https://github.com/ccomrade/cw-mod-sdk/blob/98c3dad11af50c298a5564a4b50c053a5ca11bf6/Code/Mod/GameRules.cpp#L549):
OnClientConnect добавляет channelId в m_channelIds, уведомляет synched storage и
shot validator, вызывает Lua OnClientConnect, фиксирует gameplay event и отправляет
ClEnteredGame адресно клиенту. OnClientEnteredGame (651) затем вызывает Lua с актёром,
синхронизирует storage и настраивает voice groups.

**Конкретный null-dereference:**
[ServerSynchedStorage.cpp:267](https://github.com/ccomrade/cw-mod-sdk/blob/98c3dad11af50c298a5564a4b50c053a5ca11bf6/Code/Mod/ServerSynchedStorage.cpp#L267)
получает GetNetChannel(channelId), затем в ветке нового канала вызывает
`pNetChannel->IsLocal()` без проверки. Вызов OnClientConnect для выдуманного ID
с null net channel непригоден как путь регистрации бота.

GetPlayerCount, GetPlayer и GetPlayers (GameRules.cpp:1224–1280) обходят m_channelIds.
IsPlayerInGame проверяет реальный INetChannel и eCVS_InGame. Поля реестров объявлены
в [GameRules.h](../Code/Mod/GameRules.h): m_channelIds, m_entityteams, m_playerteams,
m_channelteams. Наличие сущности само по себе не добавляет её в эти представления.
Один общий channelId=0 также не обеспечивает уникальную адресацию нескольких ботов.

**Вывод:** серверный актёр без соединения — обоснованное направление эксперимента;
полноценный виртуальный участник IA/PS через один SpawnPlayer — не готовая возможность.
Ненулевой фиктивный ID лишь меняет IsPlayer, не создавая INetChannel, а также рискует
столкнуться с реальными ID. План: отдельный реестр ботов по EntityId и явная интеграция
их в player enumeration, scoring, команды, respawn и late join без фиктивного connect.
Объём необходимых адаптаций определяется экспериментом, API движка не подменяется.

## Движение, действия и репликация

[Player.cpp:944](../Code/Mod/Player.cpp): выбор input зависит от channelId и IsClient:
CPlayerInput для локального игрока, CDedicatedInput для dedicated client, CNetPlayerInput
для сетевого игрока. При channelId=0 штатный input обычно не создаётся (есть исключение
demo playback). Готового BotInput здесь нет.

[IPlayerInput.h](../Code/Mod/IPlayerInput.h) задаёт SSerializedPlayerInput: направление
движения/взгляда, stance, sprint и lean; INPUT_ASPECT=eEA_GameClientDynamic.
CPlayer::NetSerialize (4141) вызывает CActor::NetSerialize, сериализует здоровье,
предмет, нанокостюм, затем input. Физические профили проходят через Actor::NetSerialize.
Серверное создание актёра не гарантирует корректную authority клиентского input-аспекта.

[NetPlayerInput.cpp](../Code/Mod/NetPlayerInput.cpp) преобразует вход в CMovementRequest
и вызывает IMovementController::RequestMovement. Его Update обнуляет старый input
по sv_input_timeout. Новый BotController должен обеспечивать непрерывные серверные
намерения и сетевое состояние; нельзя считать принудительный SetWorldTM навигацией.
В Actor::Init используется ePPU_WhenAIActivated — ещё одна точка проверки обновлений
актёра без работающего CryAI.

[Weapon.cpp](../Code/Mod/Weapon.cpp) и [WeaponClientServer.cpp](../Code/Mod/WeaponClientServer.cpp)
содержат StartFire/StopFire, RequestShoot/RequestStartFire и серверные ветки отправки
состояния клиентам. Нужно отдельно проверить spawn projectile, боезапас, попадание и
shot validation для server-owned бота. SSerializedPlayerInput не включает всю стрельбу.
Нанокостюм сериализуется до input, его расход энергии нельзя заменять только анимацией.

[GameRulesClientServer.cpp](../Code/Mod/GameRulesClientServer.cpp): SvRequestHit и
SvRequestSimpleHit получают актёра отправителя из настоящего pNetChannel. Серверные
ServerHit/ProcessServerHit вызывают shot validator и Lua OnHit. Бот должен использовать
серверную игровую цепочку, не имитировать клиентский RMI и не отключать проверки людей.

## Смерть, возрождение, команды, очки

GameRules::RevivePlayer (901) устанавливает здоровье/physics profile, позицию,
очищает инвентарь по флагу, вызывает NetReviveAt, рассылает ClRevive и gameplay event.
Выбор разрешённого спавна и времени возрождения этим методом не реализуется.
KillPlayer (1089) обрабатывает оружие/нанокостюм/инвентарь/ragdoll, вызывает NetKill,
ClKill и события смерти/убийства. Он использует приведение к CPlayer: произвольного
AI-актёра нельзя без проверки считать совместимым игроком.

SetTeam (1610) меняет entity/player/channel team maps, вызывает Lua OnSetTeam и ClSetTeam.
При этом UpdateObjectivesForPlayer и voice logic всё ещё опираются на канал.
Простое добавление EntityId в команду недостаточно для всей PS-интеграции.
GameplayRecorder и synched entity values — существующие механизмы событий/состояния,
но UI scoreboard, начисление IA-очков, Prestige Points и покупка требуют анализа Lua.
В текущей версии никакие очки или цены не выдуманы и не начисляются менеджером.

## C++ ↔ Lua IA/PS

GameRules::Init берёт таблицы Server/Client со script table сущности и устанавливает
текущие таблицы состояния; смена состояния выбирает вложенные таблицы.
[ScriptBind_GameRules.cpp:280](../Code/Mod/ScriptBind_GameRules.cpp) добавляет таблицу
`game` с делегированием методов. Зарегистрированы SpawnPlayer, RevivePlayer, KillPlayer,
GetPlayers, SetTeam, SetSynchedEntityValue и другие. C++ вызывает OnClientConnect,
OnClientEnteredGame, OnChangeTeam, OnSetTeam, OnHit и другие Lua callbacks.

Factory регистрирует IA/PS как режимы одного CGameRules. Однако исходники штатных
InstantAction.lua и PowerStruggle.lua в cw-mod-sdk и дереве AStopher не поставляются;
установленная 1.5 с её скриптами не найдена. Поэтому имена специфичных PS-функций
покупки/захвата, порядок respawn и scoreboard не объявляются проверенными.
Следующий анализ — локальные Scripts/GameRules из собственной установки 1.5,
без импорта игровых ресурсов в Git.

## Что даёт Crysis Co-op

[README](https://github.com/MikeCrossley/Crysis-Co-op/blob/138cf44e0fa134415e24ff19ff299e7198387733/README.md)
говорит, что эта ветка предназначена для базового Crysis, поддержка Wars прекращена.
Поэтому это референс подхода, а не совместимый SDK-модуль.

[CoopSystem.cpp](https://github.com/MikeCrossley/Crysis-Co-op/blob/138cf44e0fa134415e24ff19ff299e7198387733/Mods/CrysisCoop/Code/Coop/CoopSystem.cpp)
подключает level listener, выполняет AI.lua и включает обновление AI. OnLoadingStart
временно меняет bMultiplayer, вызывает Init/FlushSystem/Enable/LoadNavigationData,
затем возвращает multiplayer; OnLoadingComplete сходным образом делает Reset.
Update временно переключает режим для ForceCoopAI на технике. Initialize также
принудительно отключает sv_cheatprotection. Эти глобальные обходы не перенесены.

Этот код показывает проблему запуска AI в MP, но сам по себе не доказывает
сетевую совместимость AI с Player, scoreboard или PS. CryAI и наличие навигационных
данных на картах Wars необходимо проверять отдельно. Первый бот может использовать
собственные ограниченные waypoints, не включая глобальный singleplayer режим.
