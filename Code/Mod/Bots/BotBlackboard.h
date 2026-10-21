#pragma once
#include "BotIntent.h"
#include <IEntity.h>
class CPlayer;
namespace WarsBots
{
struct BotBlackboard
{
	int health = 0;
	Vec3 position = Vec3(ZERO), velocity = Vec3(ZERO), lastEnemyPosition = Vec3(ZERO);
	EntityId enemy = 0;
	float distance = 0, energy = 0, groundTime = 0;
	float nextJump = 0, nextSuitChange = 0, damagedUntil = 0;
	ENanoMode mode = NANOMODE_DEFENSE;
	bool suitActive = false, grounded = false, moving = false, blocked = false, visible = false;
	bool canMove = false;
	BotJump obstacleJump = BotJump::None;
	void Sample(CPlayer& actor, float now);
};
}
