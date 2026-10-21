#include "StdAfx.h"
#include "BotLocomotion.h"
#include "Player.h"
#include <IMovementController.h>

namespace WarsBots
{
namespace
{
bool Ray(CPlayer& actor, const Vec3& from, const Vec3& delta, ray_hit& hit)
{
	IItem* held = actor.GetCurrentItem();
	return gEnv->pPhysicalWorld->RayWorldIntersection(from,delta,ent_all,
		rwi_stop_at_pierceable|rwi_colltype_any,&hit,1,actor.GetEntity()->GetPhysics(),
		held ? held->GetEntity()->GetPhysics() : nullptr) != 0;
}
}
void BotLocomotion::Reset()
{
	m_movement.Reset(); m_pending=m_airborne=m_strength=false;
	m_requested=m_startZ=m_apex=m_nextLog=0;
}
void BotLocomotion::Stop(CPlayer& actor)
{
	m_movement.Stop(actor);
	actor.SetWarsBotActions(false,false,STANCE_STAND);
	const float nextLog=m_nextLog; Reset(); m_nextLog=nextLog;
}
void BotLocomotion::Refused(const char* reason, float now)
{
	if (now >= m_nextLog) { CryLogAlways("[WarsBots] ACTION REFUSED: %s.",reason); m_nextLog=now+2.0f; }
}
void BotLocomotion::Observe(CPlayer& actor, BotBlackboard& state, float now)
{
	if (state.health<=0) { if (m_pending || m_airborne) Stop(actor); return; }
	if (m_pending && !state.grounded && state.velocity.z>0.2f)
	{
		m_pending=false; m_airborne=true;
		actor.PublishWarsBotJump(m_strength);
		CryLogAlways("[WarsBots] JUMP TAKEOFF: strength=%d vz=%.2f energy=%.1f.",m_strength?1:0,state.velocity.z,state.energy);
	}
	if (m_pending && now-m_requested>1.0f) { m_pending=false; Refused("jump_not_accepted_by_native_movement",now); }
	if (m_airborne)
	{
		m_apex=max(m_apex,state.position.z);
		if (state.grounded)
		{
			CryLogAlways("[WarsBots] JUMP LANDING: strength=%d rise=%.2f displacementZ=%.2f energy=%.1f.",m_strength?1:0,m_apex-m_startZ,state.position.z-m_startZ,state.energy);
			m_airborne=false;
		}
	}
}
BotJump BotLocomotion::Obstacle(CPlayer& actor, const Vec3& direction) const
{
	ray_hit hit;
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	if (!Ray(actor,feet+Vec3(0,0,0.4f),direction*1.5f,hit)) return BotJump::None;
	if (Ray(actor,feet+Vec3(0,0,1.8f),direction*2.0f,hit)) return BotJump::None;
	const bool strength=Ray(actor,feet+Vec3(0,0,1.0f),direction*2.0f,hit);
	if (!JumpSpace(actor,direction,true,strength)) return BotJump::None;
	return strength?BotJump::Strength:BotJump::Normal;
}
bool BotLocomotion::JumpSpace(CPlayer& actor, const Vec3& direction, bool moving, bool strength) const
{
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	ray_hit hit;
	// Conservative standing clearance; never infer a safe jump just from the low obstacle ray.
	for (float side : {-0.3f,0.0f,0.3f})
		if (Ray(actor,feet+Vec3(-direction.y*side,direction.x*side,1.65f),Vec3(0,0,strength?7.0f:2.5f),hit)) return false;
	Vec3 landing=feet+direction*(moving?(strength?3.0f:2.0f):0.0f);
	if (!Ray(actor,landing+Vec3(0,0,strength?3.0f:1.0f),Vec3(0,0,strength?-4.0f:-2.0f),hit) || hit.n.z<0.75f) return false;
	landing=hit.pt;
	if (fabs_tpl(landing.z-feet.z)>(strength?1.5f:0.6f)) return false;
	if (moving && Ray(actor,feet+Vec3(0,0,strength?2.5f:1.3f),landing-feet,hit)) return false;
	primitives::sphere body; body.r=0.35f; body.center=landing+Vec3(0,0,0.4f);
	IPhysicalEntity* skip[2]; int skipCount=0;
	if (IPhysicalEntity* bodyPhysics=actor.GetEntity()->GetPhysics()) skip[skipCount++]=bodyPhysics;
	IItem* held=actor.GetCurrentItem();
	if (held && held->GetEntity()->GetPhysics()) skip[skipCount++]=held->GetEntity()->GetPhysics();
	geom_contact* contact=nullptr;
	const float collision=gEnv->pPhysicalWorld->PrimitiveWorldIntersection(body.type,&body,Vec3(0,0,1.3f),
		ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid|ent_living,&contact,0,
		(geom_colltype_player<<rwi_colltype_bit)|rwi_stop_at_pierceable,0,0,0,skip,skipCount);
	return collision<=0.001f;
}
bool BotLocomotion::Execute(CPlayer& actor, const BotIntent& intent, BotBlackboard& state, float now, float dt)
{
	if (!state.canMove) { Stop(actor); Refused("movement_state",now); return false; }
	const Vec3 direction=(intent.destination-state.position).GetNormalizedSafe(actor.GetBaseQuat().GetColumn1());
	const bool strength=intent.jump==BotJump::Strength;
	bool jump=false;
	if (intent.jump!=BotJump::None)
	{
		if (!state.grounded || m_pending || m_airborne) Refused("jump_not_grounded_or_pending",now);
		else if (intent.stance==STANCE_PRONE || actor.GetStance()==STANCE_PRONE) Refused("jump_prone",now);
		else if (now<state.nextJump) Refused("jump_cooldown",now);
		else if (strength && (!state.suitActive || state.mode!=NANOMODE_STRENGTH || state.energy<90.0f)) Refused("jump_strength_mode_or_energy",now);
		else if (!strength && state.mode==NANOMODE_STRENGTH) Refused("normal_jump_waiting_for_defense",now);
		else if (!JumpSpace(actor,direction,intent.move,strength)) Refused("jump_clearance_or_landing",now);
		else
		{
			jump=true; m_pending=true; m_strength=strength; m_requested=now;
			m_startZ=m_apex=state.position.z; state.nextJump=now+3.0f;
			CryLogAlways("[WarsBots] BOT ACTION: JUMP strength=%d energy=%.1f.",strength?1:0,state.energy);
		}
	}
	// Let a validated jump carry forward through the low obstruction; never a position change.
	const bool sprint=intent.move && intent.gait==BotGait::Sprint && intent.combat==BotCombatIntent::None &&
		state.grounded && state.suitActive && state.energy>45.0f && !jump && !m_pending && !m_airborne && !actor.IsFiring();
	bool accepted=false;
	if (intent.move)
		accepted=m_movement.Move(actor,intent.destination,intent.aim,dt,
			intent.gait==BotGait::Walk?1.3f:2.5f,intent.stance,sprint?2.7f:1.5f,jump||m_pending||m_airborne);
	else { m_movement.Stop(actor); m_movement.Aim(actor,intent.aim); }
	state.blocked=intent.move && !accepted;
	actor.SetWarsBotActions(sprint && accepted,intent.move && accepted,intent.stance);
	CMovementRequest request;
	request.SetStance(jump?STANCE_STAND:intent.stance);
	if (jump) request.SetJump();
	actor.GetMovementController()->RequestMovement(request);
	return accepted;
}
}
