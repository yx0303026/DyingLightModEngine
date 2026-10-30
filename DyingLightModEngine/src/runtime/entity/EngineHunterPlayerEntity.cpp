#include "runtime/EngineHunterPlayerEntity.h"

#include "runtime/EngineEntityRuntime.h"
#include "runtime/EngineEntityBindings.h"
#include "tracking/EngineHunterTrackingRuntime.h"
#include "tracking/EngineHunterUvShieldRuntime.h"

EngineHunterPlayerEntity::EngineHunterPlayerEntity(void *instance) noexcept : EnginePlayerEntity(instance)
{
}

bool EngineHunterPlayerEntity::IsHunter() const noexcept
{
	return IsInstance(instance_);
}

DlModEntityKindV1 EngineHunterPlayerEntity::Kind() const noexcept
{
	return DLMOD_ENTITY_KIND_HUNTER;
}

DlModResult EngineHunterPlayerEntity::ReadSnapshot(DlModHunterSnapshotV1 *snapshot) const noexcept
{
	return EngineEntityRuntime::ReadHunterSnapshot(instance_, snapshot);
}

DlModResult EngineHunterPlayerEntity::ReadBonePosition(std::uint8_t boneId, DlModVector3V1 *position) const noexcept
{
	return EngineEntityRuntime::ReadEntityBonePosition(instance_, boneId, position);
}

DlModResult EngineHunterPlayerEntity::ReadMotion(DlModHunterMotionV1 *motion) const noexcept
{
	if (!IsInstance(instance_) || motion == nullptr || motion->structSize < sizeof(DlModHunterMotionV1) || motion->abiVersion != MODSDK_ABI_VERSION)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const std::uint32_t size = motion->structSize;
	DlModHunterMotionV1 result{};
	if (!EngineHunterTrackingRuntime::ReadMotion(ControlObject(), result))
		return DLMOD_RESULT_NOT_FOUND;
	*motion = result;
	motion->structSize = size;
	return DLMOD_RESULT_SUCCESS;
}

bool EngineHunterPlayerEntity::IsInstance(const void *instance) noexcept
{
	return EngineEntityRuntime::IsHunterInstance(instance);
}

void *EngineHunterPlayerEntity::CurrentInstance() noexcept
{
	void *control = EngineHunterTrackingRuntime::GetCurrentControlObject();
	return IsInstance(control == nullptr ? nullptr : static_cast<std::uint8_t *>(control) - EngineEntityBindings::ControlObjectOffset)
		? static_cast<std::uint8_t *>(control) - EngineEntityBindings::ControlObjectOffset
		: nullptr;
}

bool EngineHunterPlayerEntity::InstallTracking() noexcept
{
	return EngineHunterTrackingRuntime::Install();
}

bool EngineHunterPlayerEntity::IsTrackingInstalled() noexcept
{
	return EngineHunterTrackingRuntime::IsInstalled();
}

bool EngineHunterPlayerEntity::InstallUvShieldTracking() noexcept
{
	return EngineHunterUvShieldRuntime::Install();
}

bool EngineHunterPlayerEntity::IsUvShieldTrackingInstalled() noexcept
{
	return EngineHunterUvShieldRuntime::IsInstalled();
}

void EngineHunterPlayerEntity::Shutdown() noexcept
{
	EngineHunterUvShieldRuntime::Shutdown();
	EngineHunterTrackingRuntime::Shutdown();
}

DlModResult EngineHunterPlayerEntity::ReadUvShield(DlModHunterUvShieldV1 *snapshot) noexcept
{
	if (snapshot == nullptr || snapshot->structSize < sizeof(DlModHunterUvShieldV1) || snapshot->abiVersion != MODSDK_ABI_VERSION)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	DlModHunterUvShieldV1 result{};
	if (!EngineHunterUvShieldRuntime::Read(result))
		return DLMOD_RESULT_NOT_FOUND;
	const std::uint32_t size = snapshot->structSize;
	*snapshot = result;
	snapshot->structSize = size;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult EngineHunterPlayerEntity::SetUvShield(std::uint32_t mode, float value) noexcept
{
	return EngineHunterUvShieldRuntime::SetOverride(mode, value);
}

DlModResult EngineHunterPlayerEntity::SetUvShieldCharges(std::int32_t charges) noexcept
{
	return EngineHunterUvShieldRuntime::SetChargeOverride(charges);
}

DlModResult EngineHunterPlayerEntity::SetUvShieldDuration(float seconds) noexcept
{
	return EngineHunterUvShieldRuntime::SetDurationOverride(seconds);
}

DlModResult EngineHunterPlayerEntity::SetUvShieldProgress(float ratio) noexcept
{
	return EngineHunterUvShieldRuntime::SetProgressRatio(ratio);
}
