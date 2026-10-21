#pragma once
#include "BotMovement.h"
#include "BotCombat.h"
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
	bool Running() const { return m_running; }
	void SetVerbose(bool enabled) { m_verbose = enabled; }
private:
	void Change(State state);
	void ResetLife();
	bool m_running = false;
	bool m_verbose = false;
	bool m_fallbackGiven = false;
	State m_state = State::SPAWNING;
	float m_now = 0, m_tick = 0, m_stateTime = 0, m_weaponSearchStarted = 0, m_nextSearch = 0, m_deadTime = 0;
	float m_nextTelemetry = 0;
	EntityId m_weaponTarget = 0;
	EntityId m_roamTarget = 0;
	BotMovement m_movement;
	BotCombat m_combat;
};
}
