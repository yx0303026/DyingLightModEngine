#pragma once

#include "sdk/EngineEntityAbi.h"

#include <cstdint>

class EngineEntity
{
public:
	/* 实体基类：封装所有实体共有的句柄、空间变换、快照和枚举能力。 */
	explicit EngineEntity(void *instance = nullptr) noexcept;
	virtual ~EngineEntity() = default;

	void *Instance() const noexcept;
	void *ControlObject() const noexcept;
	bool IsValid() const noexcept;
	virtual DlModEntityKindV1 Kind() const noexcept;
	virtual DlModResult ReadSnapshot(DlModEntitySnapshotV1 *snapshot) const noexcept;
	DlModResult SetPosition(const DlModVector3V1 *position) const noexcept;
	DlModResult SetOrientation(const DlModVector3V1 *forward, const DlModVector3V1 *up) const noexcept;
	DlModResult ReadBoneByName(const char *name, DlModVector3V1 *position) const noexcept;

	static void *FindLocalInstance() noexcept;
	static bool ReadHealth(const void *entity, float &bodyHealth, float &bloodMaskHealth, std::uint64_t &lastDamagerPlayerNetId) noexcept;
	static bool ReadEntityHealth(const void *entity, float &bodyHealth) noexcept;
	static bool IsValidHandle(const DlModEntityV1 *entity) noexcept;
	static bool IsFinite(const DlModVector3V1 &value) noexcept;
	static DlModResult EnumerateNearby(const DlModVector3V1 *center, float radius, DlModNearbyEntityV1 *entities, std::uint32_t capacity, std::uint32_t *count) noexcept;
	static DlModEntityKindV1 KindOf(const void *entity) noexcept;

protected:
	/* 类型识别：基类向派生实体提供游戏 RTTI 和模型字段判断。 */
	static std::uint8_t ReadModelId(const void *entity) noexcept;
	static bool IsPlayerInstance(const void *entity) noexcept;
	static bool IsHunterInstance(const void *entity) noexcept;
	static bool IsHumanPlayerInstance(const void *entity) noexcept;
	static bool IsHumanVisualInstance(const void *entity) noexcept;
	static bool IsZombieInstance(const void *entity) noexcept;
	static std::uint32_t ClassifyEntityType(const void *entity) noexcept;
	static void *AsControlObject(const void *entity) noexcept;
	static void *EntityFromControlObject(void *controlObject) noexcept;
	static bool ReadPosition(const void *entity, DlModVector3V1 &position) noexcept;
	static bool ReadOrientation(const void *entity, DlModVector3V1 &forward, DlModVector3V1 &up) noexcept;
	static DlModResult ReadEntitySnapshot(const void *entity, DlModEntitySnapshotV1 *snapshot) noexcept;
	static bool ReadModelObject(const void *entity, void *&model) noexcept;
	static DlModResult ReadBonePositionById(const void *entity, std::uint8_t boneId, DlModVector3V1 *position) noexcept;

	void *instance_ = nullptr;
};
