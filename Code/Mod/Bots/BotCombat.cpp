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

void BotCombat::Reset() { StopFire(); m_target = 0; m_visible = false; m_sampleTime = m_nextDiagnostic = 0; m_lastPosition.zero(); m_lastAim.zero(); m_lastSeen = 0; m_nextBurst = 0; }

void BotCombat::StopFire()
{
	if (m_firingWeapon && g_pGame)
		if (IItem* item = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_firingWeapon))
			if (IWeapon* weapon = item->GetIWeapon()) weapon->StopFire();
	m_firingWeapon = 0;
}

namespace
{
IPhysicalEntity* HeldPhysics(IActor& actor)
{
	IItem* item = actor.GetCurrentItem();
	return item ? item->GetEntity()->GetPhysics() : nullptr;
}
bool ClearRay(CActor& actor, IActor& target, const Vec3& origin, const Vec3& end, EntityId& obstruction)
{
	ray_hit hit;
	int count = gEnv->pPhysicalWorld->RayWorldIntersection(origin, end-origin,
		ent_all, rwi_stop_at_pierceable | rwi_colltype_any, &hit, 1,
		actor.GetEntity()->GetPhysics(), HeldPhysics(actor));
	if (!count || hit.pCollider == target.GetEntity()->GetPhysics()) return true;
	IEntity* entity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider);
	// Living-body and character/part physics can differ while belonging to one Player.
	if (entity && entity->GetId() == target.GetEntityId()) return true;
	obstruction = entity ? entity->GetId() : 0; // 0 includes static level geometry.
	IItem* item = entity ? g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(entity->GetId()) : nullptr;
	return item && item->GetOwnerId() == target.GetEntityId() && target.GetCurrentItem() == item;
}
Vec3 Eyes(IActor& actor)
{
	// Stance height ignores weapon animation and the victim's view direction.
	return actor.GetEntity()->GetWorldPos() + Vec3(0,0,
		clamp_tpl(static_cast<CActor&>(actor).GetStanceInfo(static_cast<CActor&>(actor).GetStance())->viewOffset.z, 0.35f, 1.8f));
}
bool Sight(CActor& actor, IActor& target, EntityId& obstruction)
{
	if (!gEnv->pPhysicalWorld) return false;
	return ClearRay(actor, target, Eyes(actor), BotCombat::AimPoint(target), obstruction) ||
		ClearRay(actor, target, Eyes(actor), Eyes(target), obstruction);
}
}
Vec3 BotCombat::AimPoint(IActor& target)
{
	const Vec3 feet = target.GetEntity()->GetWorldPos();
	return feet + (Eyes(target)-feet) * 0.75f;
}
bool BotCombat::CanSee(CActor& actor, IActor& target)
	{
	EntityId obstruction = 0;
	return Sight(actor, target, obstruction);
	}
bool BotCombat::Observe(CActor& actor, IActor& target, float now, bool verbose)
{
	EntityId obstruction = 0;
	m_sampleTime = now;
	m_visible = Sight(actor, target, obstruction);
	if (m_visible)
	{
		m_lastSeen = now;
		m_lastPosition = target.GetEntity()->GetWorldPos();
		m_lastAim = AimPoint(target);
	}
	else if (verbose && now >= m_nextDiagnostic)
	{
		const Vec3 from = Eyes(actor), to = AimPoint(target);
		CryLogAlways("[WarsBots] LOS blocked target=%u obstruction=%u eyes=(%.2f,%.2f,%.2f) chest=(%.2f,%.2f,%.2f) sample=%.2f.",
			target.GetEntityId(), obstruction, from.x,from.y,from.z,to.x,to.y,to.z,now);
		m_nextDiagnostic = now + 2.0f;
	}
	return m_visible;
}

IActor* BotCombat::FindEnemy(CActor& actor, float now, bool verbose)
{
	auto* actors = g_pGame->GetIGameFramework()->GetIActorSystem();
	IActor* old = actors->GetActor(m_target);
	if (Enemy(old, actor.GetEntityId()) &&
		(old->GetEntity()->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared() < 6400)
	{
		Observe(actor, *old, now, verbose);
		if (now - m_lastSeen <= 3.0f) return old;
	}
	StopFire();
	m_target = 0;
	m_visible = false;
	float best = 6400;
	IActorIteratorPtr iterator = actors->CreateActorIterator();
	while (IActor* candidate = iterator->Next())
	{
		if (!Enemy(candidate, actor.GetEntityId())) continue;
		float distance = (candidate->GetEntity()->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared();
		if (distance < best && Observe(actor, *candidate, now, verbose))
		{
			best = distance;
			m_target = candidate->GetEntityId();
		}
	}
	if (m_target)
	{
		m_lastSeen = now;
		IActor* found = actors->GetActor(m_target);
		Observe(actor, *found, now, false);
		CryLogAlways("[WarsBots] Enemy acquired id=%u name=%s.", m_target, found->GetEntity()->GetName());
		return found;
	}
	return nullptr;
}

bool BotCombat::Attack(CActor& actor, IActor& target, float now, bool verbose)
{
	IWeapon* weapon = BotWeapons::Current(actor);
	if (!weapon || !Enemy(&target, actor.GetEntityId()) || !CanSee(actor, target)) { StopFire(); return false; }
	// Detection can use eyes; shooting must also have a clear muzzle-to-chest ray.
	SMovementState muzzleState;
	actor.GetMovementController()->GetMovementState(muzzleState);
	EntityId obstruction = 0;
	if (!muzzleState.weaponPosition.IsValid() ||
		!ClearRay(actor, target, muzzleState.weaponPosition, AimPoint(target), obstruction))
	{ StopFire(); return false; }
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
