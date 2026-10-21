#pragma once
#include "BotMovement.h"
#include "BotBlackboard.h"
class CPlayer;
namespace WarsBots
{
// Ability executor; BotMovement retains local steering and obstacle avoidance.
class BotLocomotion
{
public:
	void Reset();
	void Stop(CPlayer& actor);
	void Observe(CPlayer& actor, BotBlackboard& state, float now);
	BotJump Obstacle(CPlayer& actor, const Vec3& direction) const;
	bool Execute(CPlayer& actor, const BotIntent& intent, BotBlackboard& state, float now, float dt);
private:
	bool JumpSpace(CPlayer& actor, const Vec3& direction, bool moving, bool strength) const;
	void Refused(const char* reason, float now);
	BotMovement m_movement;
	bool m_pending = false, m_airborne = false, m_strength = false;
	float m_requested = 0, m_startZ = 0, m_apex = 0, m_nextLog = 0;
};
}
