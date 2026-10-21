#pragma once
#include "BotLocomotion.h"
#include "BotSuit.h"
#include "BotCombat.h"
struct IConsoleCmdArgs;
class CPlayer;
namespace WarsBots
{
class BotSpawner;
class BotBrain
{
public:
	enum class State { SPAWNING, SEEKING_WEAPON, MOVING_TO_WEAPON, SEARCHING_ENEMY,
		CHASING_ENEMY, ATTACKING, DEAD, RESPAWNING };
	bool Start(BotSpawner& spawner, const Vec3* requestedPosition);
	void Stop(BotSpawner& spawner);
	void Update(BotSpawner& spawner, float dt);
	void Status() const;
	void AbilityCommand(IConsoleCmdArgs* args);
	bool Running() const { return m_running; }
	void SetVerbose(bool enabled) { m_verbose = enabled; }
private:
	void Change(State state);
	void ResetLife();
	void ExecuteIntent(CPlayer& actor, float dt);
	BotBlackboard m_blackboard;
	BotIntent m_intent;
	BotSuit m_suit;
	BotJump m_testJump = BotJump::None;
	int m_testGait = -1, m_testMode = -1;
	float m_testUntil = 0, m_testModeUntil = 0, m_testJumpAt = 0;
	Vec3 m_testGoal = Vec3(ZERO);
	bool m_running = false;
	bool m_verbose = false;
	bool m_fallbackGiven = false;
	bool m_retreating = false;
	State m_state = State::SPAWNING;
	float m_now = 0, m_tick = 0, m_stateTime = 0, m_weaponSearchStarted = 0, m_nextSearch = 0, m_deadTime = 0;
	float m_nextTelemetry = 0;
	EntityId m_weaponTarget = 0;
	Vec3 m_patrolGoal = Vec3(ZERO);
	float m_patrolUntil = 0;
	unsigned m_life = 0, m_patrolStep = 0;
	IEntityClass* m_previousWeaponClass = nullptr;
	BotLocomotion m_locomotion;
	BotCombat m_combat;
};
}
