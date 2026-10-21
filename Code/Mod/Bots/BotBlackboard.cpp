#include "StdAfx.h"
#include "BotBlackboard.h"
#include "Player.h"

namespace WarsBots
{
void BotBlackboard::Sample(CPlayer& actor, float now)
{
	const int previous = health;
	health = actor.GetHealth();
	if (previous > health && health > 0) damagedUntil = now + 3.0f;
	position = actor.GetEntity()->GetWorldPos();
	const auto& stats = *static_cast<const SPlayerStats*>(actor.GetActorStats());
	groundTime = stats.onGround;
	grounded = groundTime > 0.2f && stats.inAir <= 0.01f;
	velocity = stats.velocity;
	pe_status_dynamics dynamics;
	if (actor.GetEntity()->GetPhysics() && actor.GetEntity()->GetPhysics()->GetStatus(&dynamics)) velocity = dynamics.v;
	moving = velocity.GetLengthSquared2D() > 0.09f;
	canMove = health > 0 && !actor.IsFrozen() && !stats.isRagDoll && !stats.isOnLadder &&
		!stats.inFreefall && !stats.inZeroG && !stats.isGrabbed && !stats.animationControlled &&
		!stats.spectatorMode && !stats.flyMode && !actor.GetLinkedEntity() && !actor.ShouldSwim();
	CNanoSuit* suit = actor.GetNanoSuit();
	suitActive = suit && suit->IsActive();
	energy = suit ? suit->GetSuitEnergy() : 0;
	mode = suit ? suit->GetMode() : NANOMODE_DEFENSE;
}
}
