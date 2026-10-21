#include "StdAfx.h"
#include "BotBrain.h"
#include "BotSpawner.h"
#include "BotWeapons.h"
#include "Game.h"
#include "GameRules.h"
#include "Actor.h"
#include <IWeapon.h>

namespace WarsBots
{
namespace
{
const char* StateName(BotBrain::State state)
{
	switch (state)
	{
	case BotBrain::State::SPAWNING: return "SPAWNING";
	case BotBrain::State::SEEKING_WEAPON: return "SEEKING_WEAPON";
	case BotBrain::State::MOVING_TO_WEAPON: return "MOVING_TO_WEAPON";
	case BotBrain::State::SEARCHING_ENEMY: return "SEARCHING_ENEMY";
	case BotBrain::State::CHASING_ENEMY: return "CHASING_ENEMY";
	case BotBrain::State::ATTACKING: return "ATTACKING";
	case BotBrain::State::DEAD: return "DEAD";
	case BotBrain::State::RESPAWNING: return "RESPAWNING";
	}
	return "UNKNOWN";
}
}

void BotBrain::Change(State state)
{
	if (m_state == state) return;
	m_state = state;
	m_stateTime = m_now;
	CryLogAlways("[WarsBots] AI state=%s.", StateName(state));
}

void BotBrain::ResetLife()
{
	m_movement.Reset();
	m_combat.Reset();
	m_weaponTarget = m_roamTarget = 0;
	m_fallbackGiven = false;
	m_weaponSearchStarted = m_now;
	m_nextSearch = m_now;
	m_deadTime = 0;
}

bool BotBrain::Start(BotSpawner& spawner, const Vec3* requestedPosition)
{
	if (m_running || !spawner.CanRunLocalIA())
	{
		CryLogAlways("[WarsBots] AI start refused: already running or local IA test gate is closed.");
		return false;
	}
	CryLogAlways("[WarsBots] AI state=SPAWNING (explicit server start command).");
	if (!spawner.GetActor())
	{
		Vec3 position(ZERO);
		if (requestedPosition) position = *requestedPosition;
		else
		{
			CGameRules* rules = g_pGame->GetGameRules();
			IEntity* spawn = rules ? gEnv->pEntitySystem->GetEntity(rules->GetFirstSpawnLocation()) : nullptr;
			if (!spawn) { CryLogAlways("[WarsBots] AI start failed: no IA spawn; provide wb_bot_start x y z."); return false; }
			position = spawn->GetWorldPos() + Vec3(0,0,0.2f);
		}
		if (!spawner.Spawn(position)) return false;
	}
	m_running = true;
	m_now = m_tick = 0;
	m_nextTelemetry = 0;
	m_state = State::SPAWNING;
	ResetLife();
	spawner.GetActor()->GetGameObject()->EnablePrePhysicsUpdate(ePPU_Always);
	Change(State::SEEKING_WEAPON);
	return true;
}

void BotBrain::Stop(BotSpawner& spawner)
{
	if (!m_running) return;
	m_running = false; // Cancel respawn before touching entity/weapon lifetime.
	m_combat.Reset();
	if (CActor* actor = spawner.GetActor())
	{
		m_movement.Stop(*actor);
		actor->GetGameObject()->EnablePrePhysicsUpdate(ePPU_WhenAIActivated);
	}
	CryLogAlways("[WarsBots] AI stopped; automatic respawn cancelled.");
}

void BotBrain::Status() const
{
	CryLogAlways("[WarsBots] AI running=%d state=%s target=%u weaponTarget=%u verbose=%d.",
		m_running ? 1 : 0, StateName(m_state), m_combat.Target(), m_weaponTarget, m_verbose ? 1 : 0);
}

void BotBrain::Update(BotSpawner& spawner, float dt)
{
	if (!m_running) return;
	if (!spawner.CanRunLocalIA() || !spawner.GetActor()) { Stop(spawner); return; }
	dt = clamp_tpl(dt, 0.0f, 0.25f);
	m_now += dt;
	CActor& actor = *spawner.GetActor();
	if (m_verbose && m_now >= m_nextTelemetry)
	{
		const Vec3 position = actor.GetEntity()->GetWorldPos();
		IWeapon* weapon = BotWeapons::Current(actor);
		IFireMode* mode = weapon ? weapon->GetFireMode(weapon->GetCurrentFireMode()) : nullptr;
		CryLogAlways("[WarsBots] AI telemetry EntityId=%u state=%s pos=(%.2f, %.2f, %.2f) health=%d weapon=%u ammo=%d target=%u.",
			actor.GetEntityId(), StateName(m_state), position.x, position.y, position.z, actor.GetHealth(),
			actor.GetCurrentItem() ? actor.GetCurrentItem()->GetEntityId() : 0, mode ? mode->GetAmmoCount() : -1, m_combat.Target());
		IActorIteratorPtr candidates = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
		while (IActor* candidate = candidates->Next())
			if (candidate->GetChannelId() && !strcmp(candidate->GetEntity()->GetClass()->GetName(), "Player"))
				CryLogAlways("[WarsBots] AI candidate name=%s id=%u health=%d spectator=%d hidden=%d netChannel=%d distance=%.1f visible=%d.",
					candidate->GetEntity()->GetName(), candidate->GetEntityId(), candidate->GetHealth(),
					static_cast<CActor*>(candidate)->GetSpectatorMode(), candidate->GetEntity()->IsHidden() ? 1 : 0,
					g_pGame->GetIGameFramework()->GetNetChannel(candidate->GetChannelId()) ? 1 : 0,
					(candidate->GetEntity()->GetWorldPos() - position).GetLength(), BotCombat::CanSee(actor, *candidate) ? 1 : 0);
		m_nextTelemetry = m_now + 2.0f;
	}
	if (actor.GetHealth() <= 0)
	{
		m_combat.StopFire();
		m_movement.Stop(actor);
		if (m_state != State::DEAD) { Change(State::DEAD); m_deadTime = 0; }
		m_deadTime += dt;
		if (m_deadTime >= 5.0f)
		{
			Change(State::RESPAWNING);
			if (spawner.Revive())
			{
				ResetLife();
				actor.GetGameObject()->EnablePrePhysicsUpdate(ePPU_Always);
				Change(State::SEEKING_WEAPON);
			}
			else { Change(State::DEAD); m_deadTime = 0; }
		}
		return;
	}
	m_tick += dt;
	if (m_tick < 0.1f) return; // Bound entity searches, raycasts and decisions to 10 Hz.
	const float decisionTime = m_tick;
	m_tick = 0;
	if (m_state == State::DEAD || m_state == State::RESPAWNING) { ResetLife(); Change(State::SEEKING_WEAPON); }

	if (!BotWeapons::SelectAvailable(actor))
	{
		m_combat.StopFire();
		if (m_now >= m_nextSearch && !m_weaponTarget)
		{
			Change(State::SEEKING_WEAPON);
			m_weaponTarget = BotWeapons::FindNearby(actor, 30.0f);
			m_nextSearch = m_now + 2.0f;
		}
		IEntity* item = gEnv->pEntitySystem->GetEntity(m_weaponTarget);
		if (item && m_now - m_weaponSearchStarted < 6.0f)
		{
			Change(State::MOVING_TO_WEAPON);
			if ((item->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared() <= 6.25f)
			{
				m_movement.Stop(actor);
				if (BotWeapons::PickUp(actor, m_weaponTarget)) { m_weaponTarget = 0; Change(State::SEARCHING_ENEMY); }
				else m_weaponTarget = 0;
			}
			else m_movement.Move(actor, item->GetWorldPos(), item->GetWorldPos(), decisionTime);
		}
		else if (!m_fallbackGiven && (!item || m_now - m_weaponSearchStarted >= 6.0f))
		{
			m_movement.Stop(actor);
			m_fallbackGiven = true; // At most one test supply per life, never per frame.
			if (BotWeapons::GiveTestSCAR(actor)) Change(State::SEARCHING_ENEMY);
		}
		else { m_weaponTarget = 0; m_movement.Stop(actor); Change(State::SEEKING_WEAPON); }
		return;
	}

	IActor* enemy = m_combat.FindEnemy(actor, m_now);
	if (!enemy)
	{
		Change(State::SEARCHING_ENEMY);
		CGameRules* rules = g_pGame->GetGameRules();
		IEntity* roam = gEnv->pEntitySystem->GetEntity(m_roamTarget);
		if (!roam || (roam->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared2D() < 4 || m_now - m_stateTime > 8)
		{
			int count = rules->GetSpawnLocationCount();
			if (count) m_roamTarget = rules->GetSpawnLocation(static_cast<int>(m_now / 4.0f) % count);
			m_stateTime = m_now;
			roam = gEnv->pEntitySystem->GetEntity(m_roamTarget);
		}
		if (roam) m_movement.Move(actor, roam->GetWorldPos(), roam->GetWorldPos() + Vec3(0,0,1.3f), decisionTime);
		else m_movement.Stop(actor);
		return;
	}
	const bool visible = BotCombat::CanSee(actor, *enemy);
	const float distance = (enemy->GetEntity()->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLength();
	Vec3 aim = BotCombat::AimPoint(*enemy);
	// Small deterministic error; the weapon still supplies its own normal spread.
	const float error = min(0.25f, distance * 0.006f);
	aim += Vec3(sinf(m_now * 2.7f), cosf(m_now * 1.9f), sinf(m_now * 1.3f) * 0.3f) * error;
	if (visible && distance <= 18.0f)
	{
		m_movement.Stop(actor);
		m_movement.Aim(actor, aim);
		Change(State::ATTACKING);
		if (m_now - m_stateTime >= 0.3f) m_combat.Attack(actor, *enemy, m_now, m_verbose);
	}
	else
	{
		m_combat.StopFire();
		Change(State::CHASING_ENEMY);
		bool accepted = m_movement.Move(actor, enemy->GetEntity()->GetWorldPos(), aim, decisionTime);
		if (m_verbose && !accepted && m_now - m_stateTime > 2.0f)
		{ CryLogAlways("[WarsBots] Local chase blocked; no teleport/pathfinding fallback."); m_stateTime = m_now; }
	}
}
}
