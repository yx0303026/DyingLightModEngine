#pragma once

#include "runtime/EnginePlayerEntity.h"

class EngineHunterPlayerEntity final : public EnginePlayerEntity
{
public:
	/* 暗夜猎手玩家实体：提供猎手骨骼、运动跟踪和紫外护盾能力。 */
	explicit EngineHunterPlayerEntity(void *instance = nullptr) noexcept;
	using EnginePlayerEntity::ReadSnapshot;
	bool IsHunter() const noexcept;
	DlModEntityKindV1 Kind() const noexcept override;
	DlModResult ReadSnapshot(DlModHunterSnapshotV1 *snapshot) const noexcept;
	DlModResult ReadBonePosition(std::uint8_t boneId, DlModVector3V1 *position) const noexcept;
	DlModResult ReadMotion(DlModHunterMotionV1 *motion) const noexcept;
	static bool IsInstance(const void *instance) noexcept;
	static void *CurrentInstance() noexcept;
	static bool InstallTracking() noexcept;
	static bool IsTrackingInstalled() noexcept;
	static bool InstallUvShieldTracking() noexcept;
	static bool IsUvShieldTrackingInstalled() noexcept;
	static void Shutdown() noexcept;
	static DlModResult ReadUvShield(DlModHunterUvShieldV1 *snapshot) noexcept;
	static DlModResult SetUvShield(std::uint32_t mode, float value) noexcept;
	static DlModResult SetUvShieldCharges(std::int32_t charges) noexcept;
	static DlModResult SetUvShieldDuration(float seconds) noexcept;
	static DlModResult SetUvShieldProgress(float ratio) noexcept;
};

using EngineNightHunterPlayerEntity = EngineHunterPlayerEntity;
