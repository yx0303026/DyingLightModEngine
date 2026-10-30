#pragma once

#include "sdk/EngineEntityAbi.h"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>

class EngineEntityBindings;

/* 实体基类：持有游戏实体实例，并提供所有实体共享的读写、分类和查询能力。 */
class EngineEntity
{
public:
	explicit EngineEntity(void *instance = nullptr) noexcept;
	virtual ~EngineEntity() = default;

	void *Instance() const noexcept;
	void *ControlObject() const noexcept;
	bool IsValid() const noexcept;
	virtual DlModEntityKindV1 Kind() const noexcept;

	virtual DlModResult ReadSnapshot(
		DlModEntitySnapshotV1 *snapshot) const noexcept;
	DlModResult SetPosition(const DlModVector3V1 *position) const noexcept;
	DlModResult SetOrientation(
		const DlModVector3V1 *forward,
		const DlModVector3V1 *up) const noexcept;
	DlModResult ReadBoneByName(
		const char *name,
		DlModVector3V1 *position) const noexcept;

	static void *FindLocalInstance() noexcept;
	static bool ReadHealth(
		const void *entity,
		float &bodyHealth,
		float &bloodMaskHealth,
		std::uint64_t &lastDamagerPlayerNetId) noexcept;
	static bool ReadEntityHealth(
		const void *entity,
		float &bodyHealth) noexcept;
	static bool IsValidHandle(const DlModEntityV1 *entity) noexcept;
	static bool IsFinite(const DlModVector3V1 &value) noexcept;
	static DlModResult EnumerateNearby(
		const DlModVector3V1 *center,
		float radius,
		DlModNearbyEntityV1 *entities,
		std::uint32_t capacity,
		std::uint32_t *count) noexcept;

protected:
	/* 实体类型分类标记：缓存 RTTI 遍历结果，供派生实体复用。 */
	enum EntityClassFlags : std::uint32_t
	{
		EntityClassPlayer = 0x00000001u,
		EntityClassHumanVisual = 0x00000002u,
		EntityClassZombieVisual = 0x00000004u
	};

	void *instance_ = nullptr;

	static std::uint8_t ReadModelId(const void *entity) noexcept;
	static bool IsPlayerInstance(const void *entity) noexcept;
	static bool IsHunterInstance(const void *entity) noexcept;
	static bool IsHumanPlayerInstance(const void *entity) noexcept;
	static bool IsHumanVisualInstance(const void *entity) noexcept;
	static bool IsZombieInstance(const void *entity) noexcept;
	static std::uint32_t ClassifyEntityType(const void *entity) noexcept;
	static DlModEntityKindV1 EntityKindOf(const void *entity) noexcept;
	static void *AsControlObject(const void *entity) noexcept;
	static void *EntityFromControlObject(void *controlObject) noexcept;
	static bool ReadPosition(
		const void *entity,
		DlModVector3V1 &position) noexcept;
	static bool ReadOrientation(
		const void *entity,
		DlModVector3V1 &forward,
		DlModVector3V1 &up) noexcept;
	static DlModResult ReadEntitySnapshot(
		const void *entity,
		DlModEntitySnapshotV1 *snapshot) noexcept;
	static bool ReadModelObject(
		const void *entity,
		void *&model) noexcept;
	static DlModResult ReadBonePositionById(
		const void *entity,
		std::uint8_t boneId,
		DlModVector3V1 *position) noexcept;

private:
	/* 游戏对象集合布局：由 FindObjectsInRadius 返回并由游戏侧 UCRT 释放。 */
	struct ControlObjectCollection
	{
		void **items = nullptr;
		std::uint32_t count = 0;
		std::uint32_t capacity = 0;
		std::uint32_t flags = 0;
	};

	struct HealthCacheEntry
	{
		const void *entity = nullptr;
		void *module = nullptr;
	};

	struct LocalEntityCache
	{
		const void *entity = nullptr;
		std::uint16_t replicationId = 0xFFFF;
		ULONGLONG updatedAt = 0;
	};

	struct EntityClassEntry
	{
		std::uintptr_t vtable = 0;
		std::uint32_t flags = 0;
	};

	static constexpr std::uint16_t kInvalidReplicationId = 0xFFFF;
	static constexpr float kLocalSearchRadius = 500.0F;
	static constexpr std::size_t kHealthCacheCapacity = 64;
	static constexpr std::size_t kEntityClassCacheSize = 64;

	static std::array<HealthCacheEntry, kHealthCacheCapacity> healthCache_;
	static std::size_t healthCacheCursor_;
	static SRWLOCK healthCacheLock_;
	static LocalEntityCache localEntityCache_;
	static SRWLOCK localEntityCacheLock_;
	static thread_local std::array<EntityClassEntry, kEntityClassCacheSize>
		entityClassCache_;
	static thread_local std::size_t entityClassCursor_;
	static thread_local std::uintptr_t lastEntityVtable_;
	static thread_local std::uint32_t lastEntityFlags_;

	static bool ReadReplicationId(
		const EngineEntityBindings &bindings,
		const void *entity,
		std::uint16_t &replicationId) noexcept;
	static void *FindActiveLevel(
		const EngineEntityBindings &bindings) noexcept;
	static void *FindHealthModule(const void *entity) noexcept;
	static void WriteNearbyPass(
		const EngineEntityBindings &bindings,
		const ControlObjectCollection &collection,
		bool actorsOnly,
		DlModNearbyEntityV1 *entities,
		std::uint32_t capacity,
		std::uint32_t &written) noexcept;
	static void FreeGameBuffer(void *block) noexcept;
	static DlModVector3V1 DeriveUp(
		const DlModVector3V1 &forward,
		const DlModVector3V1 &up) noexcept;

	template <typename Value>
	static bool ReadField(
		const void *object,
		std::uintptr_t offset,
		Value &value) noexcept;
};
