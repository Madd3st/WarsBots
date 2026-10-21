#include "StdAfx.h"
#include "BotCombat.h"
#include "BotWeapons.h"
#include "Actor.h"
#include "Game.h"
#include <IActorSystem.h>
#include <IItemSystem.h>
#include <IWeapon.h>
#include <IMovementController.h>

namespace WarsBots
{
namespace
{
bool Enemy(IActor* target, EntityId self)
{
	return target && target->GetEntityId() != self && target->GetChannelId() != 0 &&
		target->GetHealth() > 0 && !strcmp(target->GetEntity()->GetClass()->GetName(), "Player") &&
		static_cast<CActor*>(target)->GetSpectatorMode() == 0 &&
		!target->GetEntity()->IsHidden() && !target->GetLinkedVehicle() &&
		g_pGame->GetIGameFramework()->GetNetChannel(target->GetChannelId()) != nullptr;
}
}

void BotCombat::Reset() { StopFire(); m_target = 0; m_lastSeen = 0; m_nextBurst = 0; }

void BotCombat::StopFire()
{
	if (m_firingWeapon && g_pGame)
		if (IItem* item = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_firingWeapon))
			if (IWeapon* weapon = item->GetIWeapon()) weapon->StopFire();
	m_firingWeapon = 0;
}

Vec3 BotCombat::AimPoint(IActor& target)
{
	SMovementState state;
	if (target.GetMovementController())
	{
		target.GetMovementController()->GetMovementState(state);
		if (state.eyePosition.IsValid() && (state.eyePosition - target.GetEntity()->GetWorldPos()).GetLengthSquared() < 9.0f)
			return state.eyePosition - Vec3(0,0,0.3f);
	}
	return target.GetEntity()->GetWorldPos() + Vec3(0,0,1.2f);
}

bool BotCombat::CanSee(CActor& actor, IActor& target)
{
	if (!gEnv->pPhysicalWorld) return false;
	SMovementState state;
	actor.GetMovementController()->GetMovementState(state);
	Vec3 origin = state.weaponPosition;
	if (!origin.IsValid() || (origin - actor.GetEntity()->GetWorldPos()).GetLengthSquared() > 9.0f ||
		origin.z < actor.GetEntity()->GetWorldPos().z + 0.4f)
		origin = actor.GetEntity()->GetWorldPos() + Vec3(0,0,1.4f);
	ray_hit hit;
	int count = gEnv->pPhysicalWorld->RayWorldIntersection(origin, AimPoint(target) - origin,
		ent_all, rwi_stop_at_pierceable | rwi_colltype_any, &hit, 1, actor.GetEntity()->GetPhysics());
	return count == 0 || hit.pCollider == target.GetEntity()->GetPhysics();
}

IActor* BotCombat::FindEnemy(CActor& actor, float now)
{
	auto* actors = g_pGame->GetIGameFramework()->GetIActorSystem();
	IActor* old = actors->GetActor(m_target);
	if (Enemy(old, actor.GetEntityId()) &&
		(old->GetEntity()->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared() < 6400)
	{
		if (CanSee(actor, *old)) m_lastSeen = now;
		if (now - m_lastSeen <= 3.0f) return old;
	}
	StopFire();
	m_target = 0;
	float best = 6400;
	IActorIteratorPtr iterator = actors->CreateActorIterator();
	while (IActor* candidate = iterator->Next())
	{
		if (!Enemy(candidate, actor.GetEntityId())) continue;
		float distance = (candidate->GetEntity()->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared();
		if (distance < best && CanSee(actor, *candidate))
		{
			best = distance;
			m_target = candidate->GetEntityId();
		}
	}
	if (m_target)
	{
		m_lastSeen = now;
		IActor* found = actors->GetActor(m_target);
		CryLogAlways("[WarsBots] Enemy acquired id=%u name=%s.", m_target, found->GetEntity()->GetName());
		return found;
	}
	return nullptr;
}

bool BotCombat::Attack(CActor& actor, IActor& target, float now, bool verbose)
{
	IWeapon* weapon = BotWeapons::Current(actor);
	if (!weapon || !Enemy(&target, actor.GetEntityId()) || !CanSee(actor, target)) { StopFire(); return false; }
	if (BotWeapons::ReloadIfNeeded(actor, *weapon) || !BotWeapons::HasAmmunition(actor, *weapon))
	{ StopFire(); return false; }
	SMovementState state;
	actor.GetMovementController()->GetMovementState(state);
	Vec3 direction = (AimPoint(target) - state.weaponPosition).GetNormalizedSafe(FORWARD_DIRECTION);
	if (direction.Dot(state.fireDirection.GetNormalizedSafe(FORWARD_DIRECTION)) < 0.96f)
	{ StopFire(); return false; }
	if (m_firingWeapon && now >= m_burstEnd) StopFire();
	if (!m_firingWeapon && now >= m_nextBurst && weapon->CanFire())
	{
		m_firingWeapon = actor.GetCurrentItem()->GetEntityId();
		m_burstEnd = now + 0.2f;
		m_nextBurst = now + 0.65f;
		weapon->StartFire();
		if (verbose) CryLogAlways("[WarsBots] Stock weapon burst requested weapon=%u target=%u.", m_firingWeapon, target.GetEntityId());
	}
	return m_firingWeapon != 0;
}
}
