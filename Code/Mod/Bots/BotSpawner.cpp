// WarsBots experiment. See LICENSE.txt. Uses the existing actor network binding.
#include "StdAfx.h"
#include "BotSpawner.h"
#include "Game.h"
#include "GameRules.h"
#include "Actor.h"
#include "WeaponSystem.h"
#include "Projectile.h"
#include <IItemSystem.h>
#include <IActorSystem.h>
#include <IGameRulesSystem.h>
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace WarsBots
{
namespace
{
bool ParseCoordinate(const char* text, float& value)
{
	if (!text || !*text)
		return false;
	char* end = nullptr;
	errno = 0;
	value = std::strtof(text, &end);
	return end != text && *end == '\0' && errno != ERANGE &&
	       std::isfinite(value) && std::fabs(value) <= 1000000.0f;
}
}

void BotSpawner::Initialize(IGameFramework* framework)
{
	if (m_framework || !framework)
		return;
	m_framework = framework;
	m_levelReady = framework->GetILevelSystem()->IsLevelLoaded();
	framework->GetILevelSystem()->AddListener(this);
	framework->RegisterListener(this, "WarsBotsProbe", FRAMEWORKLISTENERPRIORITY_GAME);
	gEnv->pEntitySystem->AddSink(this);
	gEnv->pConsole->Register("wb_probe_local_test", &m_localTestEnabled, 0, 0,
		"Opt in to manual actor probes; requires sv_bind=127.0.0.1 and sv_lanonly=1.");
	gEnv->pConsole->AddCommand("wb_probe_spawn", SpawnCommand, 0, "Server only: wb_probe_spawn x y z");
	gEnv->pConsole->AddCommand("wb_probe_remove", RemoveCommand, 0, "Remove only the owned WarsBots probe");
	gEnv->pConsole->AddCommand("wb_probe_status", StatusCommand, 0, "Report the owned probe; not proof of client visibility");
	gEnv->pConsole->AddCommand("wb_probe_revive", ReviveCommand, 0, "Local IA only: revive the owned dead probe at its original spawn position");
	gEnv->pConsole->AddCommand("wb_bot_start", BotStartCommand, 0, "Local IA: wb_bot_start [x y z]; explicit opt-in autonomous combat");
	gEnv->pConsole->AddCommand("wb_bot_stop", BotStopCommand, 0, "Stop movement/fire/automatic respawn; leave the actor for probe commands");
	gEnv->pConsole->AddCommand("wb_bot_status", BotStatusCommand, 0, "Report autonomous bot state");
	gEnv->pConsole->AddCommand("wb_bot_verbose", BotVerboseCommand, 0, "wb_bot_verbose 0|1");
	CryLogAlways("[WarsBots] Experimental spawner compiled; runtime gate wb_probe_local_test defaults to 0; no automatic spawn.");
}

void BotSpawner::Shutdown()
{
	if (!m_framework)
		return;
	gEnv->pConsole->RemoveCommand("wb_probe_spawn");
	gEnv->pConsole->RemoveCommand("wb_probe_remove");
	gEnv->pConsole->RemoveCommand("wb_probe_status");
	gEnv->pConsole->RemoveCommand("wb_probe_revive");
	gEnv->pConsole->RemoveCommand("wb_bot_start");
	gEnv->pConsole->RemoveCommand("wb_bot_stop");
	gEnv->pConsole->RemoveCommand("wb_bot_status");
	gEnv->pConsole->RemoveCommand("wb_bot_verbose");
	gEnv->pConsole->UnregisterVariable("wb_probe_local_test", true);
	EndLevel();
	gEnv->pEntitySystem->RemoveSink(this);
	m_framework->GetILevelSystem()->RemoveListener(this);
	m_framework->UnregisterListener(this);
	m_framework = nullptr;
}

bool BotSpawner::Spawn(const Vec3& position)
{
	ICVar* bind = gEnv->pConsole->GetCVar("sv_bind");
	ICVar* lan = gEnv->pConsole->GetCVar("sv_lanonly");
	if (m_localTestEnabled != 1 || !bind || strcmp(bind->GetString(), "127.0.0.1") != 0 ||
	    !lan || lan->GetIVal() != 1)
	{
		CryLogAlways("[WarsBots] Spawn refused: set wb_probe_local_test=1 on a loopback LAN server (sv_bind=127.0.0.1, sv_lanonly=1).");
		return false;
	}
	if (!m_framework || !gEnv->bServer || !gEnv->bMultiplayer || gEnv->bEditor ||
	    !m_levelReady || !m_framework->GetILevelSystem()->IsLevelLoaded() ||
	    !m_framework->GetIGameRulesSystem()->GetCurrentGameRules())
	{
		CryLogAlways("[WarsBots] Spawn refused: requires an active multiplayer server level.");
		return false;
	}
	if (m_entityId || gEnv->pEntitySystem->FindEntityByName("WarsBot_01"))
	{
		CryLogAlways("[WarsBots] Spawn refused: probe already exists or name WarsBot_01 is occupied.");
		return false;
	}

	// Direct creation avoids SpawnPlayer's lookup by the shared channel ID zero.
	// CActor::Init performs BindToNetwork; do not bind twice or fabricate a channel.
	IActor* actor = m_framework->GetIActorSystem()->CreateActor(
		0, "WarsBot_01", "Player", position, Quat(IDENTITY), Vec3(1, 1, 1));
	if (!actor)
	{
		CryLogAlways("[WarsBots] CreateActor failed; no match registration attempted.");
		return false;
	}
	m_entityId = actor->GetEntityId();
	m_identity = actor->GetEntity();
	const Vec3 actualPosition = actor->GetEntity()->GetWorldPos();
	m_lastSpawnLocation = 0;
	if (CGameRules* rules = g_pGame->GetGameRules())
		for (int i=0; i<rules->GetSpawnLocationCount(); ++i)
			if (IEntity* spawn = gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(i)))
				if ((spawn->GetWorldPos()-actualPosition).GetLengthSquared()<2.25f)
				{ m_lastSpawnLocation = spawn->GetId(); break; }
	m_spawnPosition = actualPosition;
	m_spawnAngles = Ang3(actor->GetEntity()->GetWorldRotation());
	m_lastHealth = actor->GetHealth();
	m_lastHitShooter = 0;
	m_killPlayerObserved = false;
	CryLogAlways("[WarsBots] Probe created EntityId=%u class=%s position=(%.3f, %.3f, %.3f); client visibility UNVERIFIED.",
		m_entityId, actor->GetEntity()->GetClass()->GetName(), actualPosition.x, actualPosition.y, actualPosition.z);
	Status();
	return true;
}

void BotSpawner::Remove()
{
	m_brain.Stop(*this);
	if (!m_entityId)
		return;
	const EntityId id = m_entityId;
	IEntity* entity = gEnv->pEntitySystem->GetEntity(id);
	if (entity != m_identity || !entity)
	{
		CryLogAlways("[WarsBots] Probe identity expired EntityId=%u; no entity removed.", id);
		m_entityId = 0;
		m_identity = nullptr;
		return;
	}
	// Only the entity created by this instance; never delete by name or channel.
	gEnv->pEntitySystem->RemoveEntity(id, true);
	if (!gEnv->pEntitySystem->GetEntity(id))
	{
		m_entityId = 0;
		m_identity = nullptr;
		CryLogAlways("[WarsBots] Probe removed EntityId=%u.", id);
	}
	else
		CryLogAlways("[WarsBots] Probe removal pending or refused EntityId=%u; do not spawn another probe.", id);
}

void BotSpawner::Status() const
{
	IActor* actor = m_framework && m_entityId ? m_framework->GetIActorSystem()->GetActor(m_entityId) : nullptr;
	if (!actor || actor->GetEntity() != m_identity)
	{
		CryLogAlways("[WarsBots] No live owned probe.");
		return;
	}
	CryLogAlways("[WarsBots] EntityId=%u channelId=%u IsPlayer=%d health=%d inventory=%d physics=%d movement=%d; match participant=NO.",
		m_entityId, static_cast<unsigned>(actor->GetChannelId()), actor->IsPlayer() ? 1 : 0,
		actor->GetHealth(), actor->GetInventory() ? 1 : 0, actor->GetEntity()->GetPhysics() ? 1 : 0,
		actor->GetMovementController() ? 1 : 0);
	CryLogAlways("[WarsBots] Lifecycle health=%d physicsProfile=%u lastHitShooter=%u KillPlayerObserved=%d.",
		actor->GetHealth(), static_cast<unsigned>(actor->GetGameObject()->GetAspectProfile(eEA_Physics)),
		m_lastHitShooter, m_killPlayerObserved ? 1 : 0);
}

bool BotSpawner::Owns(EntityId id) const
{
	return id != 0 && id == m_entityId && m_identity && gEnv && gEnv->bServer &&
		gEnv->pEntitySystem->GetEntity(id) == m_identity;
}

bool BotSpawner::CanRunLocalIA() const
{
	if (!m_framework || !gEnv || !gEnv->bServer || !gEnv->bMultiplayer || gEnv->bEditor ||
		!m_levelReady || !m_framework->GetILevelSystem()->IsLevelLoaded() || m_localTestEnabled != 1 ||
		!g_pGame->GetGameRules()) return false;
	ICVar* bind = gEnv->pConsole->GetCVar("sv_bind");
	ICVar* lan = gEnv->pConsole->GetCVar("sv_lanonly");
	ICVar* mode = gEnv->pConsole->GetCVar("sv_gamerules");
	return bind && strcmp(bind->GetString(), "127.0.0.1") == 0 && lan && lan->GetIVal() == 1 &&
		mode && strcmp(mode->GetString(), "InstantAction") == 0;
}

CActor* BotSpawner::GetActor() const
{
	if (!m_framework || !Owns(m_entityId)) return nullptr;
	IActor* actor = m_framework->GetIActorSystem()->GetActor(m_entityId);
	return actor && !strcmp(actor->GetEntity()->GetClass()->GetName(), "Player") ? static_cast<CActor*>(actor) : nullptr;
}

bool BotSpawner::IsOwnedProjectileHit(const HitInfo& hit) const
{
	// In-flight server bullets remain valid after AI/gate shutdown; no new shots are enabled here.
	if (!m_framework || hit.remote || !Owns(hit.shooterId) || !hit.projectileId) return false;
	CProjectile* projectile = g_pGame->GetWeaponSystem()->GetProjectile(hit.projectileId);
	IItem* weapon = m_framework->GetIItemSystem()->GetItem(hit.weaponId);
	return projectile && !projectile->IsRemote() && projectile->GetOwnerId() == hit.shooterId &&
		weapon && weapon->GetOwnerId() == hit.shooterId;
}

void BotSpawner::TraceHit(const char* phase, const HitInfo& hit)
{
	if (!m_framework || !gEnv->bServer)
		return;
	if (strcmp(phase, "before-Lua") == 0 && Owns(hit.targetId))
		m_lastHitShooter = hit.shooterId;
	IActor* actor = m_framework->GetIActorSystem()->GetActor(hit.targetId);
	IEntity* shooter = gEnv->pEntitySystem->GetEntity(hit.shooterId);
	CGameRules* rules = g_pGame->GetGameRules();
	// Keys read from the installed IA script; read-only diagnostics, not score awards.
	int kills = 0, deaths = 0;
	const bool hasKills = rules && hit.shooterId && rules->GetSynchedEntityValue(hit.shooterId, 100, kills);
	const bool hasDeaths = rules && rules->GetSynchedEntityValue(hit.targetId, 101, deaths);
	CryLogAlways("[WarsBots] Hit %s target=%u shooter=%u name=%s weapon=%u damage=%.3f health=%d shooterKills=%d(valid=%d) targetDeaths=%d(valid=%d).",
		phase, hit.targetId, hit.shooterId, shooter ? shooter->GetName() : "<none>", hit.weaponId,
		hit.damage, actor ? actor->GetHealth() : -1, kills, hasKills ? 1 : 0, deaths, hasDeaths ? 1 : 0);
}

void BotSpawner::TraceKill(EntityId shooterId, bool killerEventEmitted)
{
	m_killPlayerObserved = true;
	IEntity* shooter = gEnv->pEntitySystem->GetEntity(shooterId);
	CryLogAlways("[WarsBots] GameRules::KillPlayer target=%u killer=%u name=%s; eGE_Death emitted; eGE_Kill emitted=%d (not proof of scoreboard credit).",
		m_entityId, shooterId, shooter ? shooter->GetName() : "<none>", killerEventEmitted ? 1 : 0);
}

void BotSpawner::OnPostUpdate(float dt)
{
	m_brain.Update(*this, dt);
	if (!m_framework || !Owns(m_entityId))
		return;
	IActor* actor = m_framework->GetIActorSystem()->GetActor(m_entityId);
	if (!actor)
		return;
	const int health = actor->GetHealth();
	if (m_lastHealth > 0 && health <= 0)
		CryLogAlways("[WarsBots] Death observed EntityId=%u health=%d lastHitShooter=%u KillPlayerObserved=%d (last hit alone is not killer attribution).",
			m_entityId, health, m_lastHitShooter, m_killPlayerObserved ? 1 : 0);
	else if (m_lastHealth <= 0 && health > 0)
		CryLogAlways("[WarsBots] Alive transition observed EntityId=%u health=%d.", m_entityId, health);
	m_lastHealth = health;
}

bool BotSpawner::Revive()
{
	ICVar* bind = gEnv->pConsole->GetCVar("sv_bind");
	ICVar* lan = gEnv->pConsole->GetCVar("sv_lanonly");
	ICVar* mode = gEnv->pConsole->GetCVar("sv_gamerules");
	if (!m_framework || !Owns(m_entityId) || !gEnv->bMultiplayer || gEnv->bEditor ||
		!m_levelReady || !m_framework->GetILevelSystem()->IsLevelLoaded() || m_localTestEnabled != 1 ||
		!bind || strcmp(bind->GetString(), "127.0.0.1") != 0 || !lan || lan->GetIVal() != 1 ||
		!mode || strcmp(mode->GetString(), "InstantAction") != 0)
	{
		CryLogAlways("[WarsBots] Revive refused: requires owned probe and enabled local IA test server.");
		return false;
	}
	CGameRules* rules = g_pGame->GetGameRules();
	CActor* actor = static_cast<CActor*>(m_framework->GetIActorSystem()->GetActor(m_entityId));
	if (!rules || !actor || actor->GetHealth() > 0 || actor->GetChannelId() != 0 ||
		strcmp(actor->GetEntity()->GetClass()->GetName(), "Player") != 0 || !actor->GetInventory() ||
		actor->GetLinkedVehicle() || rules->IsFrozen(m_entityId))
	{
		CryLogAlways("[WarsBots] Revive refused: probe must be a dead, unfrozen Player with inventory and no vehicle.");
		return false;
	}
	const EntityId id = m_entityId;
	EntityId chosen = 0;
	const int count = rules->GetSpawnLocationCount();
	const int start = count ? (m_entityId + static_cast<unsigned>(gEnv->pTimer->GetFrameStartTime().GetMilliSeconds())) % count : 0;
	// Native checks include a swept body volume and proximity to other Player entities.
	// Last spawn is a fallback only when no alternative passes both checks.
	for (int pass=0; pass<2 && !chosen; ++pass)
		for (int i=0; i<count; ++i)
		{
			EntityId candidate = rules->GetSpawnLocation((start+i)%count);
			if ((!pass && candidate==m_lastSpawnLocation) ||
				!rules->IsSpawnLocationSafe(id,candidate,6.0f,0.0f) ||
				!rules->TestSpawnLocationWithEnvironment(candidate,id,0.2f,1.8f)) continue;
			chosen = candidate;
			break;
		}
	if (!chosen)
	{
		CryLogAlways("[WarsBots] Revive deferred: no safe IA spawn; retry in 5 seconds.");
		return false; // Safer fallback than reviving inside geometry or a player.
	}
	IEntity* spawn = gEnv->pEntitySystem->GetEntity(chosen);
	if (!spawn) return false;
	const Vec3 position = spawn->GetWorldPos()+Vec3(0,0,0.2f);
	const Ang3 angles(spawn->GetWorldRotation());
	CryLogAlways("[WarsBots] Revive requested EntityId=%u spawnId=%u previousSpawn=%u position=(%.3f,%.3f,%.3f).",
		id,chosen,m_lastSpawnLocation,position.x,position.y,position.z);
	rules->RevivePlayer(actor,position,angles,0,true);

	IActor* revived = m_framework->GetIActorSystem()->GetActor(id);
	const bool alive = Owns(id) && revived && revived->GetHealth() > 0 &&
		revived->GetGameObject()->GetAspectProfile(eEA_Physics) == eAP_Alive;
	CryLogAlways("[WarsBots] Revive result EntityId=%u sameEntity=%d alive=%d health=%d; client visibility requires manual verification.",
		id, Owns(id) ? 1 : 0, alive ? 1 : 0, revived ? revived->GetHealth() : -1);
	if (alive)
	{
		m_lastSpawnLocation = chosen;
		m_lastHealth = revived->GetHealth();
		m_lastHitShooter = 0;
		m_killPlayerObserved = false;
	}
	return alive;
}

void BotSpawner::OnEvent(IEntity* entity, SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_DONE && entity && entity->GetId() == m_entityId && entity == m_identity)
	{
		m_brain.Stop(*this);
		CryLogAlways("[WarsBots] Probe ENTITY_EVENT_DONE EntityId=%u.", m_entityId);
		m_entityId = 0;
		m_identity = nullptr;
	}
}

void BotSpawner::EndLevel() { m_brain.Stop(*this); m_levelReady = false; Remove(); }
void BotSpawner::OnLevelNotFound(const char*) { EndLevel(); }
void BotSpawner::OnLoadingStart(ILevelInfo*) { EndLevel(); }
void BotSpawner::OnLoadingComplete(ILevel*) { m_levelReady = true; }
void BotSpawner::OnLoadingError(ILevelInfo*, const char*) { EndLevel(); }
void BotSpawner::OnLoadGame(ILoadGame*) { EndLevel(); }
void BotSpawner::OnLevelEnd(const char*) { EndLevel(); }

void BotSpawner::OnActionEvent(const SActionEvent& event)
{
	if (event.m_event == eAE_resetBegin || event.m_event == eAE_disconnected)
		EndLevel();
	else if (event.m_event == eAE_resetEnd && m_framework)
		m_levelReady = m_framework->GetILevelSystem()->IsLevelLoaded();
}

void BotSpawner::SpawnCommand(IConsoleCmdArgs* args)
{
	Vec3 position(ZERO);
	if (!args || args->GetArgCount() != 4 || !ParseCoordinate(args->GetArg(1), position.x) ||
	    !ParseCoordinate(args->GetArg(2), position.y) || !ParseCoordinate(args->GetArg(3), position.z))
	{
		CryLogAlways("[WarsBots] Usage: wb_probe_spawn x y z (finite world coordinates).");
		return;
	}
	if (g_pGame)
		g_pGame->GetBotManager().GetSpawner().Spawn(position);
}

void BotSpawner::RemoveCommand(IConsoleCmdArgs* args)
{
	if (args && args->GetArgCount() == 1 && g_pGame && gEnv->bServer)
		g_pGame->GetBotManager().GetSpawner().Remove();
}

void BotSpawner::StatusCommand(IConsoleCmdArgs* args)
{
	if (args && args->GetArgCount() == 1 && g_pGame)
		g_pGame->GetBotManager().GetSpawner().Status();
}

void BotSpawner::ReviveCommand(IConsoleCmdArgs* args)
{
	if (args && args->GetArgCount() == 1 && g_pGame)
		g_pGame->GetBotManager().GetSpawner().Revive();
}

void BotSpawner::BotStartCommand(IConsoleCmdArgs* args)
{
	if (!args || !g_pGame) return;
	Vec3 position(ZERO);
	if (args->GetArgCount() == 1)
	{
		auto& spawner = g_pGame->GetBotManager().GetSpawner();
		spawner.m_brain.Start(spawner, nullptr);
	}
	else if (args->GetArgCount() == 4 && ParseCoordinate(args->GetArg(1), position.x) &&
		ParseCoordinate(args->GetArg(2), position.y) && ParseCoordinate(args->GetArg(3), position.z))
	{
		auto& spawner = g_pGame->GetBotManager().GetSpawner();
		spawner.m_brain.Start(spawner, &position);
	}
	else CryLogAlways("[WarsBots] Usage: wb_bot_start [x y z].");
}

void BotSpawner::BotStopCommand(IConsoleCmdArgs* args)
{
	if (args && args->GetArgCount() == 1 && g_pGame && gEnv->bServer)
	{
		auto& spawner = g_pGame->GetBotManager().GetSpawner();
		spawner.m_brain.Stop(spawner);
	}
}

void BotSpawner::BotStatusCommand(IConsoleCmdArgs* args)
{
	if (args && args->GetArgCount() == 1 && g_pGame)
	{
		auto& spawner = g_pGame->GetBotManager().GetSpawner();
		spawner.m_brain.Status();
		spawner.Status();
	}
}

void BotSpawner::BotVerboseCommand(IConsoleCmdArgs* args)
{
	if (args && args->GetArgCount() == 2 && g_pGame && gEnv->bServer &&
		(!strcmp(args->GetArg(1), "0") || !strcmp(args->GetArg(1), "1")))
		g_pGame->GetBotManager().GetSpawner().m_brain.SetVerbose(!strcmp(args->GetArg(1), "1"));
	else CryLogAlways("[WarsBots] Usage: wb_bot_verbose 0|1.");
}
}
