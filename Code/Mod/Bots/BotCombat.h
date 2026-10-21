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
	IActor* FindEnemy(CActor& actor, float now, bool verbose = false);
	static Vec3 AimPoint(IActor& target);
	static bool CanSee(CActor& actor, IActor& target);
	bool Attack(CActor& actor, IActor& target, float now, bool verbose);
	EntityId Target() const { return m_target; }
	bool Visible() const { return m_visible; }
	const Vec3& LastPosition() const { return m_lastPosition; }
	const Vec3& LastAim() const { return m_lastAim; }
	float SampleTime() const { return m_sampleTime; }
private:
	bool Observe(CActor& actor, IActor& target, float now, bool verbose);
	bool m_visible = false;
	Vec3 m_lastPosition = Vec3(ZERO), m_lastAim = Vec3(ZERO);
	float m_sampleTime = 0, m_nextDiagnostic = 0;
	EntityId m_target = 0;
	EntityId m_firingWeapon = 0;
	float m_lastSeen = 0;
	float m_nextBurst = 0;
	float m_burstEnd = 0;
};
}
