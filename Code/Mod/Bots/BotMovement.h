// WarsBots local movement prototype. See LICENSE.txt.
#pragma once
#include <Cry_Math.h>
class CActor;
namespace WarsBots
{
class BotMovement
{
public:
	void Reset();
	void Stop(CActor& actor);
	// Returns whether a safe local movement request was accepted, not arrival.
	bool Move(CActor& actor, const Vec3& destination, const Vec3& lookAt, float dt);
	void Aim(CActor& actor, const Vec3& target);
private:
	bool SafeStep(CActor& actor, const Vec3& direction) const;
	Vec3 m_sample = Vec3(ZERO);
	float m_sampleTime = 0;
	float m_avoidTime = 0;
	float m_side = 1;
};
}
