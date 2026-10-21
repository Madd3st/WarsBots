#pragma once
#include "BotBlackboard.h"
class CPlayer;
namespace WarsBots
{
class BotSuit
{
public:
	void Reset() { m_nextLog = 0; }
	bool Apply(CPlayer& actor, ENanoMode mode, BotBlackboard& state, float now);
	static bool CloakEnabled();
private:
	float m_nextLog = 0;
};
}
