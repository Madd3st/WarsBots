// All pickup, selection and firing go through the SDK item/weapon interfaces.
#pragma once
#include <Cry_Math.h>
#include <IEntity.h>
class CActor;
struct IEntityClass;
struct IWeapon;
namespace WarsBots
{
struct BotWeapons
{
	static IWeapon* Current(CActor& actor);
	static bool SelectAvailable(CActor& actor);
	static EntityId FindNearby(CActor& actor, float radius, IEntityClass* previousClass, unsigned life);
	static bool PickUp(CActor& actor, EntityId item);
	static bool GiveTestSCAR(CActor& actor);
	static bool HasAmmunition(CActor& actor, IWeapon& weapon);
	static bool ReloadIfNeeded(CActor& actor, IWeapon& weapon);
};
}
