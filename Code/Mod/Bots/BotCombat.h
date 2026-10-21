#pragma once
#include <Cry_Math.h>
#include <IEntity.h>
class CActor;
struct IActor;
namespace WarsBots
{
class BotCombat
{
public:
	void Reset();
	void StopFire();
	IActor* FindEnemy(CActor& actor, float now);
	static Vec3 AimPoint(IActor& target);
	static bool CanSee(CActor& actor, IActor& target);
	bool Attack(CActor& actor, IActor& target, float now, bool verbose);
	EntityId Target() const { return m_target; }
private:
	EntityId m_target = 0;
	EntityId m_firingWeapon = 0;
	float m_lastSeen = 0;
	float m_nextBurst = 0;
	float m_burstEnd = 0;
};
}
