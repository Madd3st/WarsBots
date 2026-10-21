#include "StdAfx.h"
#include "BotMovement.h"
#include "Actor.h"
#include "Player.h"
#include <IItemSystem.h>
#include <IMovementController.h>

namespace WarsBots
{
void BotMovement::Reset() { m_sampleTime = 0; m_avoidTime = 0; m_side = 1; }

void BotMovement::Stop(CActor& actor)
{
	static_cast<CPlayer&>(actor).SetWarsBotMotion(actor.GetEntity()->GetWorldRotation().GetColumn1(),0,
		actor.GetEntity()->GetWorldPos()+actor.GetEntity()->GetWorldRotation().GetColumn1()*10.0f+Vec3(0,0,1.4f));
	if (auto* controller = actor.GetMovementController())
	{
		CMovementRequest request;
		request.ClearMoveTarget();
		request.SetDesiredSpeed(0);
		request.SetPseudoSpeed(0);
		controller->RequestMovement(request);
	}
}

void BotMovement::Aim(CActor& actor, const Vec3& target)
{
	static_cast<CPlayer&>(actor).SetWarsBotMotion(target-actor.GetEntity()->GetWorldPos(),0,target);
	if (auto* controller = actor.GetMovementController())
	{
		CMovementRequest request;
		request.SetLookTarget(target);
		request.SetAimTarget(target);
		request.SetFireTarget(target);
		request.SetBodyTarget(target);
		request.SetStance(STANCE_STAND);
		request.SetAllowStrafing(true);
		controller->RequestMovement(request);
	}
}

bool BotMovement::SafeStep(CActor& actor, const Vec3& direction) const
{
	if (!gEnv->pPhysicalWorld || !actor.GetEntity()->GetPhysics())
		return false;
	const Vec3 feet = actor.GetEntity()->GetWorldPos();
	IItem* held = actor.GetCurrentItem();
	IPhysicalEntity* heldPhysics = held ? held->GetEntity()->GetPhysics() : nullptr;
	ray_hit hit;
	const unsigned flags = rwi_stop_at_pierceable | rwi_colltype_any;
	// Three parallel body-width rays prevent a shoulder clipping a corner.
	for (float side : {-0.3f,0.0f,0.3f})
	for (float height : {0.4f,1.1f,1.6f})
		if (gEnv->pPhysicalWorld->RayWorldIntersection(feet + Vec3(-direction.y*side,direction.x*side,height), direction * 1.5f,
			ent_all, flags, &hit, 1, actor.GetEntity()->GetPhysics(), heldPhysics))
			return false;
	// Stop at ledges; this is deliberately not a pathfinder or a jump controller.
	return gEnv->pPhysicalWorld->RayWorldIntersection(feet + direction * 1.2f + Vec3(0,0,0.6f),
		Vec3(0,0,-1.4f), ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid,
		flags, &hit, 1, actor.GetEntity()->GetPhysics()) != 0;
}

bool BotMovement::Move(CActor& actor, const Vec3& destination, const Vec3& lookAt, float dt)
{
	auto* controller = actor.GetMovementController();
	if (!controller) return false;
	const Vec3 feet = actor.GetEntity()->GetWorldPos();
	Vec3 direction = destination - feet;
	direction.z = 0;
	if (direction.GetLengthSquared() < 0.8f * 0.8f || fabs_tpl(destination.z - feet.z) > 3.0f)
	{
		Stop(actor);
		Aim(actor, lookAt);
		return false;
	}
	direction.NormalizeSafe();
	if (m_sampleTime == 0) m_sample = feet;
	m_sampleTime += dt;
	if (m_sampleTime >= 2.0f)
	{
		if ((feet - m_sample).GetLengthSquared2D() < 0.09f)
		{
			m_avoidTime = 1.2f;
			m_side = -m_side;
		}
		m_sampleTime = 0;
	}
	m_avoidTime = max(0.0f, m_avoidTime - dt);
	bool safe = m_avoidTime <= 0 && SafeStep(actor, direction);
	if (!safe)
	{
		for (float angle : {m_side * 0.8f, -m_side * 0.8f, m_side * 1.57f, -m_side * 1.57f})
		{
			Vec3 candidate = Quat::CreateRotationZ(angle) * direction;
			if (SafeStep(actor, candidate)) { direction = candidate; safe = true; break; }
		}
	}
	if (!safe) { Stop(actor); Aim(actor, lookAt); return false; }
	static_cast<CPlayer&>(actor).SetWarsBotMotion(direction,2.5f,lookAt);
	CMovementRequest request;
	request.SetMoveTarget(feet + direction * 2.0f);
	request.SetDesiredSpeed(2.5f);
	request.SetPseudoSpeed(0.5f);
	request.SetStance(STANCE_STAND);
	request.SetLookTarget(lookAt);
	request.SetAimTarget(lookAt);
	request.SetFireTarget(lookAt);
	request.SetBodyTarget(feet + direction * 4.0f);
	request.SetAllowStrafing(false);
	return controller->RequestMovement(request);
}
}
