#include "runtime/entity/EngineZombieEntity.h"

#include "runtime/EngineEntityRuntime.h"

EngineZombieEntity::EngineZombieEntity(void *instance) noexcept : EngineEntity(instance)
{
}

bool EngineZombieEntity::IsZombie() const noexcept
{
	return IsInstance(instance_);
}

DlModEntityKindV1 EngineZombieEntity::Kind() const noexcept
{
	return DLMOD_ENTITY_KIND_ZOMBIE;
}

bool EngineZombieEntity::IsInstance(const void *instance) noexcept
{
	return EngineEntityRuntime::IsZombieInstance(instance);
}
