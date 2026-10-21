#pragma once
#include <Cry_Math.h>
#include <IAgent.h>
struct IGameFramework;
#include "NanoSuit.h"

namespace WarsBots
{
enum class BotGait { Walk, Run, Sprint };
enum class BotJump { None, Normal, Strength };
enum class BotCombatIntent { None, Aim, Fire };

// A value describing a decision; no physics or entity mutations here.
struct BotIntent
{
	bool move = false;
	Vec3 destination = Vec3(ZERO), aim = Vec3(ZERO);
	BotGait gait = BotGait::Run;
	BotJump jump = BotJump::None;
	EStance stance = STANCE_STAND;
	ENanoMode suitMode = NANOMODE_DEFENSE;
	BotCombatIntent combat = BotCombatIntent::None;
	void MoveTo(const Vec3& point, const Vec3& look) { move = true; destination = point; aim = look; }
	void AimAt(const Vec3& look) { move = false; aim = look; combat = BotCombatIntent::Aim; }
};
}
