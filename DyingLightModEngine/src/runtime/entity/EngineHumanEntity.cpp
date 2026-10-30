#include "runtime/entity/EngineHumanEntity.h"

#include "runtime/EngineEntityRuntime.h"

EngineHumanEntity::EngineHumanEntity(void *instance) noexcept : EngineEntity(instance)
{
}

bool EngineHumanEntity::IsHuman() const noexcept
{
	return IsInstance(instance_);
}

DlModEntityKindV1 EngineHumanEntity::Kind() const noexcept
{
	return DLMOD_ENTITY_KIND_HUMAN;
}

bool EngineHumanEntity::IsInstance(const void *instance) noexcept
{
	return EngineEntityRuntime::IsHumanVisualInstance(instance);
}
