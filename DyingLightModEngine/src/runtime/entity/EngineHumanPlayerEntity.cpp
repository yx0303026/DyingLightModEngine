#include "runtime/EngineHumanPlayerEntity.h"

#include "runtime/EngineEntityRuntime.h"

EngineHumanPlayerEntity::EngineHumanPlayerEntity(void *instance) noexcept : EnginePlayerEntity(instance)
{
}

bool EngineHumanPlayerEntity::IsHumanPlayer() const noexcept
{
	return IsInstance(instance_);
}

DlModEntityKindV1 EngineHumanPlayerEntity::Kind() const noexcept
{
	return DLMOD_ENTITY_KIND_HUMAN;
}

DlModResult EngineHumanPlayerEntity::ReadSnapshot(DlModHumanSnapshotV1 *snapshot) const noexcept
{
	return EngineEntityRuntime::ReadHumanSnapshot(instance_, snapshot);
}

bool EngineHumanPlayerEntity::IsInstance(const void *instance) noexcept
{
	return EngineEntityRuntime::IsHumanPlayerInstance(instance);
}

void *EngineHumanPlayerEntity::FindLocalInstance() noexcept
{
	void *instance = EnginePlayerEntity::FindLocalInstance();
	return IsInstance(instance) ? instance : nullptr;
}
