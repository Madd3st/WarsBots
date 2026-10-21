#include "StdAfx.h"
#include "BotBrain.h"
#include "BotSpawner.h"
#include "BotWeapons.h"
#include "Game.h"
#include "GameRules.h"
#include "Actor.h"
#include "Player.h"
#include <IWeapon.h>
#include <cmath>
#include <cstdlib>

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
	m_locomotion.Reset();
	m_suit.Reset(); m_blackboard=BotBlackboard(); m_intent=BotIntent();
	m_testJump=BotJump::None; m_testGait=m_testMode=-1; m_testUntil=m_testModeUntil=m_testJumpAt=0;
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
		m_blackboard.Sample(static_cast<CPlayer&>(*actor),m_now);
		if (m_blackboard.canMove && m_blackboard.suitActive)
			m_suit.Apply(static_cast<CPlayer&>(*actor),NANOMODE_DEFENSE,m_blackboard,m_now);
		m_locomotion.Stop(static_cast<CPlayer&>(*actor));
		m_testJump=BotJump::None; m_testGait=m_testMode=-1; m_testUntil=m_testModeUntil=m_testJumpAt=0;
		actor->GetGameObject()->EnablePrePhysicsUpdate(ePPU_WhenAIActivated);
	}
	m_intent=BotIntent(); m_blackboard.nextJump=m_blackboard.nextSuitChange=0; m_suit.Reset();
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
	CPlayer& actor = static_cast<CPlayer&>(*spawner.GetActor());
	m_blackboard.Sample(actor,m_now);
	m_locomotion.Observe(actor,m_blackboard,m_now);
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
		int clients=0;
		IActorIteratorPtr connected=g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
		while (IActor* client=connected->Next())
			if (client->GetChannelId() && g_pGame->GetIGameFramework()->GetNetChannel(client->GetChannelId())) ++clients;
		CryLogAlways("[WarsBots] ABILITY PHYSICS: speed=%.2f vz=%.2f grounded=%d sprint=%d mode=%d energy=%.1f active=%d clients=%d.",
			m_blackboard.velocity.GetLength2D(),m_blackboard.velocity.z,m_blackboard.grounded?1:0,
			static_cast<const SPlayerStats*>(actor.GetActorStats())->bSprinting?1:0,int(m_blackboard.mode),m_blackboard.energy,m_blackboard.suitActive?1:0,clients);
		m_nextTelemetry = m_now + 2.0f;
	}
	if (actor.GetHealth() <= 0)
	{
		m_combat.StopFire();
		m_locomotion.Stop(actor);
		m_testJump=BotJump::None; m_testGait=m_testMode=-1; m_testUntil=m_testModeUntil=m_testJumpAt=0;
		if (m_state != State::DEAD)
		{
			Change(State::DEAD); m_deadTime=0; m_intent=BotIntent();
			m_blackboard.nextJump=m_blackboard.nextSuitChange=0; m_suit.Reset();
		}
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

	m_intent=BotIntent();
	m_intent.aim=actor.GetEntity()->GetWorldPos()+actor.GetBaseQuat().GetColumn1()*10.0f+Vec3(0,0,1.4f);
	if (m_now<m_testUntil)
	{
		if (m_testGait>=0) { m_intent.MoveTo(m_testGoal,m_testGoal+Vec3(0,0,1.4f)); m_intent.gait=static_cast<BotGait>(m_testGait); }
		m_intent.jump=m_now>=m_testJumpAt?m_testJump:BotJump::None;
		if (m_testJump!=BotJump::None) m_intent.suitMode=m_testJump==BotJump::Strength?NANOMODE_STRENGTH:NANOMODE_DEFENSE;
		else if (m_testGait==int(BotGait::Sprint)) m_intent.suitMode=NANOMODE_SPEED;
		if (m_now>=m_testJumpAt) m_testJump=BotJump::None; // Consume the command once, including a refused attempt.
		ExecuteIntent(actor,decisionTime); return;
	}
	m_testGait=-1;
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
				m_intent.move=false;
				if (BotWeapons::PickUp(actor, m_weaponTarget)) { m_previousWeaponClass = actor.GetCurrentItem()->GetEntity()->GetClass(); m_weaponTarget = 0; Change(State::SEARCHING_ENEMY); }
				else m_weaponTarget = 0;
			}
			else m_intent.MoveTo(item->GetWorldPos(),item->GetWorldPos()+Vec3(0,0,1.2f));
		}
		else if (!m_fallbackGiven && (!item || m_now - m_weaponSearchStarted >= 6.0f))
		{
			m_intent.move=false;
			m_fallbackGiven = true; // At most one test supply per life, never per frame.
			if (BotWeapons::GiveTestSCAR(actor)) Change(State::SEARCHING_ENEMY);
		}
		else { m_weaponTarget = 0; m_intent.move=false; Change(State::SEEKING_WEAPON); }
		ExecuteIntent(actor,decisionTime);
		return;
	}

	IActor* enemy = m_combat.FindEnemy(actor, m_now, m_verbose);
	if (!enemy)
	{
		Change(State::SEARCHING_ENEMY);
		m_blackboard.enemy=0; m_blackboard.visible=false; m_blackboard.distance=0;
		const Vec3 feet = actor.GetEntity()->GetWorldPos();
		if (m_now >= m_patrolUntil || (m_patrolGoal-feet).GetLengthSquared2D() < 2.0f)
		{
			// Persistent local patrol legs, not map spawn-point cycling. SafeStep gates every move.
			const float angle = (++m_patrolStep * 2.399963f);
			m_patrolGoal = feet + Vec3(cosf(angle),sinf(angle),0) * (6.0f + (m_patrolStep%3)*2.0f);
			m_patrolUntil = m_now + 7.0f;
		}
		m_intent.MoveTo(m_patrolGoal,m_patrolGoal+Vec3(0,0,1.3f));
		m_intent.gait=BotGait::Run;
		ExecuteIntent(actor,decisionTime);
		if (m_blackboard.blocked) m_patrolUntil=min(m_patrolUntil,m_now+1.0f);

		return;
	}
	const bool visible = m_combat.Visible();
	const Vec3 destination = m_combat.LastPosition();
	m_blackboard.enemy=m_combat.Target(); m_blackboard.visible=visible;
	m_blackboard.lastEnemyPosition=destination;
	const float distance = (destination - actor.GetEntity()->GetWorldPos()).GetLength();
	m_blackboard.distance=distance;
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
			m_intent.MoveTo(actor.GetEntity()->GetWorldPos()+away*3.0f,aim);
			m_intent.combat=BotCombatIntent::Aim;
		}
		else { m_retreating = false; m_intent.move=false; m_intent.AimAt(aim); }
		Change(State::ATTACKING);
		m_intent.combat=m_now-m_stateTime>=0.3f?BotCombatIntent::Fire:BotCombatIntent::Aim;
	}
	else
	{
		m_combat.StopFire();
		Change(State::CHASING_ENEMY);
		m_intent.MoveTo(destination,aim);
		m_intent.gait=distance>24.0f && m_now-m_stateTime>1.5f && m_blackboard.energy>(m_blackboard.mode==NANOMODE_SPEED?50.0f:100.0f)?BotGait::Sprint:BotGait::Run;
		if (m_intent.gait==BotGait::Sprint) m_intent.suitMode=NANOMODE_SPEED;
		if (!visible && distance>12.0f && distance<30.0f && BotSuit::CloakEnabled() && m_blackboard.energy>120.0f)
		{ m_intent.suitMode=NANOMODE_CLOAK; m_intent.gait=BotGait::Walk; }

	}
	ExecuteIntent(actor,decisionTime);
}
void BotBrain::ExecuteIntent(CPlayer& actor, float dt)
{
	if (m_now<m_testModeUntil && m_testMode>=0 && m_intent.combat==BotCombatIntent::None && m_intent.jump==BotJump::None)
		m_intent.suitMode=static_cast<ENanoMode>(m_testMode);
	m_blackboard.obstacleJump=BotJump::None;
	if (m_intent.move && m_blackboard.grounded && m_now>=m_blackboard.nextJump && m_now>=m_testUntil)
	{
		const Vec3 direction=(m_intent.destination-m_blackboard.position).GetNormalizedSafe(FORWARD_DIRECTION);
		m_blackboard.obstacleJump=m_locomotion.Obstacle(actor,direction);
		if (m_blackboard.obstacleJump!=BotJump::None && m_intent.combat==BotCombatIntent::None)
		{
			m_intent.jump=m_blackboard.obstacleJump;
			m_intent.suitMode=m_intent.jump==BotJump::Strength?NANOMODE_STRENGTH:NANOMODE_DEFENSE;
	}
}
	if (!m_blackboard.grounded && static_cast<const SPlayerStats*>(actor.GetActorStats())->jumped && m_blackboard.mode==NANOMODE_STRENGTH && m_intent.jump==BotJump::None)
		m_intent.suitMode=NANOMODE_STRENGTH;
	if (m_now<m_blackboard.damagedUntil || m_intent.combat!=BotCombatIntent::None || m_blackboard.energy<45.0f)
	{ m_intent.suitMode=NANOMODE_DEFENSE; if (m_intent.gait==BotGait::Sprint) m_intent.gait=BotGait::Run; }
	const bool suitReady=m_suit.Apply(actor,m_intent.suitMode,m_blackboard,m_now);
	if (!suitReady && m_intent.gait==BotGait::Sprint) m_intent.gait=BotGait::Run;
	m_locomotion.Execute(actor,m_intent,m_blackboard,m_now,dt);
	if (m_intent.combat==BotCombatIntent::Fire && m_blackboard.mode!=NANOMODE_CLOAK)
	{
		IActor* target=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_combat.Target());
		if (target) m_combat.Attack(actor,*target,m_now,m_verbose); else m_combat.StopFire();
}
	else m_combat.StopFire();
}
void BotBrain::AbilityCommand(IConsoleCmdArgs* args)
{
	const char* command=args->GetArg(0);
	const char* value=args->GetArgCount()>=2?args->GetArg(1):"";
	CPlayer* actor=static_cast<CPlayer*>(g_pGame->GetBotManager().GetSpawner().GetActor());
	if (!actor) return;
	if (!strcmp(command,"wb_bot_abilities"))
	{
		m_blackboard.Sample(*actor,m_now);
		CryLogAlways("[WarsBots] ABILITIES health=%d grounded=%d speed=%.2f vz=%.2f mode=%d energy=%.1f active=%d jumpCooldown=%.2f suitCooldown=%.2f.",
			m_blackboard.health,m_blackboard.grounded?1:0,m_blackboard.velocity.GetLength2D(),m_blackboard.velocity.z,
			int(m_blackboard.mode),m_blackboard.energy,m_blackboard.suitActive?1:0,
			max(0.0f,m_blackboard.nextJump-m_now),max(0.0f,m_blackboard.nextSuitChange-m_now));
		return;
	}
	if (actor->GetHealth()<=0) { CryLogAlways("[WarsBots] ACTION REFUSED: dead actor."); return; }
	if (!strcmp(command,"wb_bot_jump") && args->GetArgCount()<=3 &&
		(args->GetArgCount()==1 || !strcmp(value,"normal") || !strcmp(value,"strength")))
	{
		if (m_testJump!=BotJump::None) { CryLogAlways("[WarsBots] ACTION REFUSED: jump command already queued."); return; }
		float delay=0;
		if (args->GetArgCount()==3)
		{
			char* end=nullptr; delay=strtof(args->GetArg(2),&end);
			if (!end || end==args->GetArg(2) || *end || !std::isfinite(delay) || delay<0 || delay>30)
			{ CryLogAlways("[WarsBots] ACTION REFUSED: jump delay must be 0..30 seconds."); return; }
		}
		m_testJump=!strcmp(value,"strength")?BotJump::Strength:BotJump::Normal;
		m_testJumpAt=m_now+delay; m_testGait=-1; m_testUntil=m_testJumpAt+6.0f;
		return;
	}
	if (!strcmp(command,"wb_bot_gait"))
	{
		if (!strcmp(value,"auto")) { m_testUntil=0; m_testGait=-1; return; }
		if (!strcmp(value,"walk")) m_testGait=int(BotGait::Walk);
		else if (!strcmp(value,"run")) m_testGait=int(BotGait::Run);
		else if (!strcmp(value,"sprint")) m_testGait=int(BotGait::Sprint);
		else { CryLogAlways("[WarsBots] Usage: wb_bot_gait walk|run|sprint|auto."); return; }
		m_testGoal=actor->GetEntity()->GetWorldPos()+actor->GetBaseQuat().GetColumn1()*25.0f;
		m_testUntil=m_now+6.0f; return;
	}
	if (!strcmp(command,"wb_bot_suit"))
	{
		if (!strcmp(value,"auto")) { m_testMode=-1; m_testModeUntil=0; return; }
		if (!strcmp(value,"defense")) m_testMode=NANOMODE_DEFENSE;
		else if (!strcmp(value,"speed")) m_testMode=NANOMODE_SPEED;
		else if (!strcmp(value,"strength")) m_testMode=NANOMODE_STRENGTH;
		else if (!strcmp(value,"cloak")) m_testMode=NANOMODE_CLOAK;
		else { CryLogAlways("[WarsBots] Usage: wb_bot_suit defense|speed|strength|cloak|auto."); return; }
		m_testModeUntil=m_now+8.0f; return;
	}
	CryLogAlways("[WarsBots] Usage: wb_bot_jump [normal|strength] [delaySeconds].");
}

}
