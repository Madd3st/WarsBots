// WarsBots project addition. See LICENSE.txt for SDK usage terms.
#pragma once

struct IGameFramework;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
#include "BotSpawner.h"
#endif

namespace WarsBots
{
// Game-lifetime bootstrap only. Match membership and bot simulation are not implemented.
class BotManager final
{
public:
	void Initialize(IGameFramework* framework);
	void Shutdown();
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
	BotSpawner& GetSpawner() { return m_spawner; }
#endif

private:
	bool m_initialized = false;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
	BotSpawner m_spawner;
#endif
};
}
