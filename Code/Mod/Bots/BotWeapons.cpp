#include "StdAfx.h"
#include "BotWeapons.h"
#include "Actor.h"
#include "Item.h"
#include "Game.h"
#include <IItemSystem.h>
#include <IWeapon.h>

namespace WarsBots
{
namespace
{
bool Suitable(IItem* item)
{
	if (!item || !item->GetIWeapon()) return false;
	const char* name = item->GetEntity()->GetClass()->GetName();
	return !strcmp(name,"SCAR") || !strcmp(name,"FY71") || !strcmp(name,"SMG") ||
		!strcmp(name,"Shotgun") || !strcmp(name,"SOCOM");
}
IItemSystem* Items() { return g_pGame->GetIGameFramework()->GetIItemSystem(); }
}

IWeapon* BotWeapons::Current(CActor& actor)
{
	IItem* item = actor.GetCurrentItem();
	return Suitable(item) && item->GetOwnerId() == actor.GetEntityId() ? item->GetIWeapon() : nullptr;
}

bool BotWeapons::HasAmmunition(CActor& actor, IWeapon& weapon)
{
	IFireMode* mode = weapon.GetFireMode(weapon.GetCurrentFireMode());
	return mode && mode->GetAmmoType() && (mode->GetAmmoCount() > 0 ||
		(actor.GetInventory() && actor.GetInventory()->GetAmmoCount(mode->GetAmmoType()) > 0));
}

bool BotWeapons::SelectAvailable(CActor& actor)
{
	if (auto* current = Current(actor))
		if (HasAmmunition(actor, *current)) return true;
	auto* inventory = actor.GetInventory();
	if (!inventory) return false;
	for (int i=0; i<inventory->GetCount(); ++i)
	{
		IItem* item = Items()->GetItem(inventory->GetItem(i));
		if (Suitable(item) && item->CanSelect() && HasAmmunition(actor, *item->GetIWeapon()))
		{
			Items()->SetActorItem(&actor, item->GetEntityId(), false);
			return Current(actor) != nullptr;
		}
	}
	return false;
}

EntityId BotWeapons::FindNearby(CActor& actor, float radius)
{
	EntityId result = 0;
	float best = radius * radius;
	IEntityItPtr iterator = gEnv->pEntitySystem->GetEntityIterator();
	while (IEntity* entity = iterator->Next())
	{
		if (entity->IsHidden()) continue;
		float distance = (entity->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared();
		if (distance >= best) continue;
		IItem* item = Items()->GetItem(entity->GetId());
		if (Suitable(item) && !item->GetOwnerId() && static_cast<CItem*>(item)->CanPickUp(actor.GetEntityId()))
		{
			best = distance;
			result = entity->GetId();
		}
	}
	return result;
}

bool BotWeapons::PickUp(CActor& actor, EntityId id)
{
	IItem* item = Items()->GetItem(id);
	if (!Suitable(item) || item->GetOwnerId() || !static_cast<CItem*>(item)->CanPickUp(actor.GetEntityId())) return false;
	if ((item->GetEntity()->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared() > 6.25f) return false;
	item->PickUp(actor.GetEntityId(), false, true, false);
	item = Items()->GetItem(id);
	const bool acquired = item && item->GetOwnerId() == actor.GetEntityId() && actor.GetInventory() &&
		actor.GetInventory()->FindItem(id) >= 0;
	CryLogAlways("[WarsBots] REAL PICKUP actor=%u item=%u acquired=%d.", actor.GetEntityId(), id, acquired ? 1 : 0);
	return acquired && SelectAvailable(actor);
}

bool BotWeapons::GiveTestSCAR(CActor& actor)
{
	EntityId id = Items()->GiveItem(&actor, "SCAR", false, true, false);
	IItem* item = Items()->GetItem(id);
	if (!item || item->GetOwnerId() != actor.GetEntityId() || !actor.GetInventory())
	{
		CryLogAlways("[WarsBots] TEST GRANT SCAR failed actor=%u item=%u.", actor.GetEntityId(), id);
		return false;
	}
	IWeapon* weapon = item->GetIWeapon();
	IFireMode* mode = weapon ? weapon->GetFireMode(weapon->GetCurrentFireMode()) : nullptr;
	if (!mode || !mode->GetAmmoType() || mode->GetClipSize() <= 0) return false;
	actor.GetInventory()->SetAmmoCapacity(mode->GetAmmoType(), max(180, actor.GetInventory()->GetAmmoCapacity(mode->GetAmmoType())));
	actor.GetInventory()->SetAmmoCount(mode->GetAmmoType(), 180);
	weapon->SetAmmoCount(mode->GetAmmoType(), mode->GetClipSize());
	Items()->SetActorItem(&actor, id, false);
	CryLogAlways("[WarsBots] TEST GRANT SCAR actor=%u item=%u clip=%d reserve=180 (not a map pickup).",
		actor.GetEntityId(), id, mode->GetAmmoCount());
	return Current(actor) != nullptr;
}

bool BotWeapons::ReloadIfNeeded(CActor& actor, IWeapon& weapon)
{
	if (weapon.IsReloading()) return true;
	IFireMode* mode = weapon.GetFireMode(weapon.GetCurrentFireMode());
	if (mode && mode->GetAmmoCount() <= 0 && actor.GetInventory() &&
		actor.GetInventory()->GetAmmoCount(mode->GetAmmoType()) > 0 && weapon.CanReload())
	{
		weapon.Reload(false);
		return true;
	}
	return false;
}
}
