#include "StdAfx.h"
#include "BotSuit.h"
#include "Player.h"

namespace WarsBots
{
bool BotSuit::CloakEnabled()
{
	ICVar* flag = gEnv->pConsole->GetCVar("wb_bot_cloak_experimental");
	return flag && flag->GetIVal() == 1;
}
bool BotSuit::Apply(CPlayer& actor, ENanoMode mode, BotBlackboard& state, float now)
{
	CNanoSuit* suit = actor.GetNanoSuit();
	const char* reason = nullptr;
	if (!state.canMove || !suit || !suit->IsActive()) reason = "suit_inactive_or_actor_unavailable";
	else if (mode == NANOMODE_CLOAK && !CloakEnabled()) reason = "cloak_experimental_disabled";
	else if (!suit->IsModeActive(mode)) reason = "mode_unavailable";
	else if (mode == NANOMODE_STRENGTH && state.energy < 90.0f) reason = "strength_energy";
	else if (mode == NANOMODE_SPEED && state.energy < (state.mode == mode ? 50.0f : 100.0f)) reason = "speed_energy";
	else if (mode == NANOMODE_CLOAK && state.energy < (state.mode == mode ? 60.0f : 120.0f)) reason = "cloak_energy";
	if (reason)
	{
		if (suit && suit->IsActive() && suit->GetMode()==NANOMODE_CLOAK && !CloakEnabled())
		{
			suit->SetMode(NANOMODE_DEFENSE);
			state.mode=suit->GetMode();
		}
		if (now >= m_nextLog) { CryLogAlways("[WarsBots] ACTION REFUSED: %s mode=%d energy=%.1f.",reason,int(mode),state.energy); m_nextLog=now+2.0f; }
		return false;
	}
	if (suit->GetMode() == mode) return true;
	// Defense can interrupt an ability immediately on attack/damage; other transitions have hysteresis.
	if (now < state.nextSuitChange && mode != NANOMODE_DEFENSE) return false;
	if (!suit->SetMode(mode)) return false;
	state.nextSuitChange = now + 2.0f;
	state.mode = suit->GetMode();
	CryLogAlways("[WarsBots] BOT SUIT: mode=%d energy=%.1f active=%d.",int(state.mode),suit->GetSuitEnergy(),suit->IsActive()?1:0);
	return state.mode == mode;
}
}
