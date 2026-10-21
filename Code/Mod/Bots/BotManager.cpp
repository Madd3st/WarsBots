// WarsBots project addition. See LICENSE.txt for SDK usage terms.
#include "StdAfx.h"
#include "BotManager.h"

namespace WarsBots
{
void BotManager::Initialize(IGameFramework* framework)
{
	if (m_initialized)
		return;

	m_initialized = true;
	CryLogAlways("[WarsBots] BotManager initialized (bootstrap only; no bots spawned).");
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
	m_spawner.Initialize(framework);
#else
	(void)framework;
#endif
}

void BotManager::Shutdown()
{
	if (!m_initialized)
		return;

	m_initialized = false;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
	m_spawner.Shutdown();
#endif
	if (gEnv && gEnv->pLog)
		CryLogAlways("[WarsBots] BotManager shut down.");
}
}
