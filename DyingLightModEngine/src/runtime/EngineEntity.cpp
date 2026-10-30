#include "runtime/EngineEntity.h"

#include "runtime/EngineEntityRuntime.h"

#include <cmath>

EngineEntity::EngineEntity(void *instance) noexcept : instance_(instance)
{
}

void *EngineEntity::Instance() const noexcept
{
	return instance_;
}

void *EngineEntity::ControlObject() const noexcept
{
	return EngineEntityRuntime::ControlObjectForInstance(instance_);
}

bool EngineEntity::IsValid() const noexcept
{
	return instance_ != nullptr;
}

DlModEntityKindV1 EngineEntity::Kind() const noexcept
{
	return EngineEntityRuntime::EntityKind(instance_);
}

DlModResult EngineEntity::ReadSnapshot(DlModEntitySnapshotV1 *snapshot) const noexcept
{
	return EngineEntityRuntime::ReadSnapshotForInstance(instance_, snapshot);
}

DlModResult EngineEntity::SetPosition(const DlModVector3V1 *position) const noexcept
{
	return EngineEntityRuntime::SetEntityPosition(instance_, position);
}

DlModResult EngineEntity::SetOrientation(const DlModVector3V1 *forward, const DlModVector3V1 *up) const noexcept
{
	return EngineEntityRuntime::SetEntityOrientation(instance_, forward, up);
}

DlModResult EngineEntity::ReadBoneByName(const char *name, DlModVector3V1 *position) const noexcept
{
	DlModEntityV1 handle{instance_};
	return EngineEntityRuntime::ReadBoneByName(&handle, name, position);
}

void *EngineEntity::FindLocalInstance() noexcept
{
	return EngineEntityRuntime::LocalEntityInstance();
}

bool EngineEntity::ReadHealth(const void *entity, float &bodyHealth, float &bloodMaskHealth, std::uint64_t &lastDamagerPlayerNetId) noexcept
{
	return EngineEntityRuntime::ReadEntityHealthDetails(entity, bodyHealth, bloodMaskHealth, lastDamagerPlayerNetId);
}

bool EngineEntity::ReadEntityHealth(const void *entity, float &bodyHealth) noexcept
{
	return EngineEntityRuntime::ReadEntityHealth(entity, bodyHealth);
}

bool EngineEntity::IsValidHandle(const DlModEntityV1 *entity) noexcept
{
	return entity != nullptr && entity->instance != nullptr;
}

bool EngineEntity::IsFinite(const DlModVector3V1 &value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

DlModResult EngineEntity::EnumerateNearby(const DlModVector3V1 *center, float radius, DlModNearbyEntityV1 *entities, std::uint32_t capacity, std::uint32_t *count) noexcept
{
	return EngineEntityRuntime::EnumerateNearby(center, radius, entities, capacity, count);
}

DlModEntityKindV1 EngineEntity::KindOf(const void *entity) noexcept
{
	return EngineEntityRuntime::EntityKind(entity);
}

std::uint8_t EngineEntity::ReadModelId(const void *entity) noexcept
{
	return EngineEntityRuntime::EntityModelId(entity);
}

bool EngineEntity::IsPlayerInstance(const void *entity) noexcept
{
	return EngineEntityRuntime::IsPlayerInstance(entity);
}

bool EngineEntity::IsHunterInstance(const void *entity) noexcept
{
	return EngineEntityRuntime::IsHunterInstance(entity);
}

bool EngineEntity::IsHumanPlayerInstance(const void *entity) noexcept
{
	return EngineEntityRuntime::IsHumanPlayerInstance(entity);
}

bool EngineEntity::IsHumanVisualInstance(const void *entity) noexcept
{
	return EngineEntityRuntime::IsHumanVisualInstance(entity);
}

bool EngineEntity::IsZombieInstance(const void *entity) noexcept
{
	return EngineEntityRuntime::IsZombieInstance(entity);
}

std::uint32_t EngineEntity::ClassifyEntityType(const void *entity) noexcept
{
	return EngineEntityRuntime::EntityClassFlags(entity);
}

void *EngineEntity::AsControlObject(const void *entity) noexcept
{
	return EngineEntityRuntime::ControlObjectForInstance(entity);
}

void *EngineEntity::EntityFromControlObject(void *controlObject) noexcept
{
	return controlObject == nullptr ? nullptr : static_cast<std::uint8_t *>(controlObject) - 0x18;
}

bool EngineEntity::ReadPosition(const void *entity, DlModVector3V1 &position) noexcept
{
	return EngineEntityRuntime::ReadPositionForInstance(entity, position);
}

bool EngineEntity::ReadOrientation(const void *entity, DlModVector3V1 &forward, DlModVector3V1 &up) noexcept
{
	return EngineEntityRuntime::ReadOrientationForInstance(entity, forward, up);
}

DlModResult EngineEntity::ReadEntitySnapshot(const void *entity, DlModEntitySnapshotV1 *snapshot) noexcept
{
	return EngineEntityRuntime::ReadSnapshotForInstance(entity, snapshot);
}

bool EngineEntity::ReadModelObject(const void *entity, void *&model) noexcept
{
	return EngineEntityRuntime::ReadModelObjectForInstance(entity, model);
}

DlModResult EngineEntity::ReadBonePositionById(const void *entity, std::uint8_t boneId, DlModVector3V1 *position) noexcept
{
	return EngineEntityRuntime::ReadEntityBonePosition(entity, boneId, position);
}
