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
	m_weaponTarget = 0;
	m_patrolUntil = 0;
	++m_life;
	m_fallbackGiven = false;
	m_retreating = false;
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
		CryLogAlways("[WarsBots] AI decision target=%u visible=%d sample=%.2f age=%.2f (cached decision; muzzle checked separately).",
			m_combat.Target(), m_combat.Visible()?1:0, m_combat.SampleTime(), m_now-m_combat.SampleTime());
		const Vec3 heading = actor.GetEntity()->GetWorldRotation().GetColumn1();
		pe_status_dynamics dynamics;
		const bool physical = actor.GetEntity()->GetPhysics() && actor.GetEntity()->GetPhysics()->GetStatus(&dynamics);
		CryLogAlways("[WarsBots] Motion heading=(%.2f,%.2f) physicalSpeed=%.2f.",heading.x,heading.y,physical?dynamics.v.GetLength2D():-1.0f);
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
			m_weaponTarget = BotWeapons::FindNearby(actor, 30.0f, m_previousWeaponClass, m_life);
			m_nextSearch = m_now + 2.0f;
		}
		IEntity* item = gEnv->pEntitySystem->GetEntity(m_weaponTarget);
		if (item && m_now - m_weaponSearchStarted < 6.0f)
		{
			Change(State::MOVING_TO_WEAPON);
			if ((item->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared() <= 6.25f)
			{
				m_movement.Stop(actor);
				if (BotWeapons::PickUp(actor, m_weaponTarget)) { m_previousWeaponClass = actor.GetCurrentItem()->GetEntity()->GetClass(); m_weaponTarget = 0; Change(State::SEARCHING_ENEMY); }
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

	IActor* enemy = m_combat.FindEnemy(actor, m_now, m_verbose);
	if (!enemy)
	{
		Change(State::SEARCHING_ENEMY);
		const Vec3 feet = actor.GetEntity()->GetWorldPos();
		if (m_now >= m_patrolUntil || (m_patrolGoal-feet).GetLengthSquared2D() < 2.0f)
		{
			// Persistent local patrol legs, not map spawn-point cycling. SafeStep gates every move.
			const float angle = (++m_patrolStep * 2.399963f);
			m_patrolGoal = feet + Vec3(cosf(angle),sinf(angle),0) * (6.0f + (m_patrolStep%3)*2.0f);
			m_patrolUntil = m_now + 7.0f;
		}
		if (!m_movement.Move(actor, m_patrolGoal, m_patrolGoal + Vec3(0,0,1.3f), decisionTime))
			m_patrolUntil = min(m_patrolUntil, m_now + 1.0f);

		return;
	}
	const bool visible = m_combat.Visible();
	const Vec3 destination = m_combat.LastPosition();
	const float distance = (destination - actor.GetEntity()->GetWorldPos()).GetLength();
	Vec3 aim = m_combat.LastAim();
	// Small deterministic error; the weapon still supplies its own normal spread.
	const float error = min(0.25f, distance * 0.006f);
	aim += Vec3(sinf(m_now * 2.7f), cosf(m_now * 1.9f), sinf(m_now * 1.3f) * 0.3f) * error;
	if (visible && distance <= (m_state == State::ATTACKING ? 22.0f : 18.0f))
	{
		// Retreat locally when crowded; obstruction checks still forbid unsafe steps.
		if (distance < (m_retreating ? 9.0f : 6.0f))
		{
			m_retreating = true;
			Vec3 away = (actor.GetEntity()->GetWorldPos()-destination).GetNormalizedSafe(FORWARD_DIRECTION);
			m_movement.Move(actor, actor.GetEntity()->GetWorldPos()+away*3.0f, aim, decisionTime);
		}
		else { m_retreating = false; m_movement.Stop(actor); m_movement.Aim(actor, aim); }
		Change(State::ATTACKING);
		if (m_now - m_stateTime >= 0.3f) m_combat.Attack(actor, *enemy, m_now, m_verbose);
	}
	else
	{
		m_combat.StopFire();
		Change(State::CHASING_ENEMY);
		bool accepted = m_movement.Move(actor, destination, aim, decisionTime);
		if (m_verbose && !accepted && m_now - m_stateTime > 2.0f)
		{ CryLogAlways("[WarsBots] Local chase blocked; no teleport/pathfinding fallback."); m_stateTime = m_now; }
	}
}
}
