#include "runtime/EnginePlayerEntity.h"

#include "runtime/EngineEntityRuntime.h"

EnginePlayerEntity::EnginePlayerEntity(void *instance) noexcept : EngineEntity(instance)
{
}

bool EnginePlayerEntity::IsPlayer() const noexcept
{
	return IsInstance(instance_);
}

DlModEntityKindV1 EnginePlayerEntity::Kind() const noexcept
{
	return DLMOD_ENTITY_KIND_PLAYER;
}

DlModResult EnginePlayerEntity::ReadSnapshot(DlModPlayerSnapshotV1 *snapshot) const noexcept
{
	return EngineEntityRuntime::ReadPlayerSnapshot(instance_, snapshot);
}

bool EnginePlayerEntity::IsInstance(const void *instance) noexcept
{
	return EngineEntityRuntime::IsPlayerInstance(instance);
}

void *EnginePlayerEntity::FindLocalInstance() noexcept
{
	void *instance = EngineEntityRuntime::LocalEntityInstance();
	return IsInstance(instance) ? instance : nullptr;
}
