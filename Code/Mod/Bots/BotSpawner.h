// WarsBots experiment. See LICENSE.txt. Not a registered match participant.
#pragma once

#include <IGameFramework.h>
#include <ILevelSystem.h>
#include <IEntitySystem.h>
#include "BotBrain.h"

struct IConsoleCmdArgs;
struct HitInfo;
class CActor;

namespace WarsBots
{
class BotSpawner final : public ILevelSystemListener,
                         public IEntitySystemSink,
                         public IGameFrameworkListener
{
public:
	void Initialize(IGameFramework* framework);
	void Shutdown();
	bool Spawn(const Vec3& position);
	void Remove();
	void Status() const;
	bool Owns(EntityId id) const;
	bool Revive();
	void TraceHit(const char* phase, const HitInfo& hit);
	void TraceKill(EntityId shooterId, bool killerEventEmitted);
	bool CanRunLocalIA() const;
	CActor* GetActor() const;
	bool AIEnabled() const { return m_brain.Running(); }
	bool IsOwnedProjectileHit(const HitInfo& hit) const;

	void OnLevelNotFound(const char*) override;
	void OnLoadingStart(ILevelInfo*) override;
	void OnLoadingComplete(ILevel*) override;
	void OnLoadingError(ILevelInfo*, const char*) override;
	void OnLoadingProgress(ILevelInfo*, int) override {}

	bool OnBeforeSpawn(SEntitySpawnParams&) override { return true; }
	void OnSpawn(IEntity*, SEntitySpawnParams&) override {}
	bool OnRemove(IEntity*) override { return true; }
	void OnEvent(IEntity* entity, SEntityEvent& event) override;

	void OnPostUpdate(float) override;
	void OnSaveGame(ISaveGame*) override {}
	void OnLoadGame(ILoadGame*) override;
	void OnLevelEnd(const char*) override;
	void OnActionEvent(const SActionEvent& event) override;

private:
	static void SpawnCommand(IConsoleCmdArgs* args);
	static void RemoveCommand(IConsoleCmdArgs* args);
	static void StatusCommand(IConsoleCmdArgs* args);
	static void ReviveCommand(IConsoleCmdArgs* args);
	static void BotStartCommand(IConsoleCmdArgs* args);
	static void BotStopCommand(IConsoleCmdArgs* args);
	static void BotStatusCommand(IConsoleCmdArgs* args);
	static void BotVerboseCommand(IConsoleCmdArgs* args);
	void EndLevel();

	IGameFramework* m_framework = nullptr;
	EntityId m_entityId = 0;
	// Identity guard only: never dereference this cached pointer.
	const IEntity* m_identity = nullptr;
	bool m_levelReady = false;
	int m_localTestEnabled = 0;
	Vec3 m_spawnPosition = Vec3(ZERO);
	Ang3 m_spawnAngles = Ang3(ZERO);
	int m_lastHealth = 0;
	EntityId m_lastHitShooter = 0;
	bool m_killPlayerObserved = false;
	BotBrain m_brain;
};
}
