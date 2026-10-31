#include "runtime/EngineEntityRuntime.h"

#include "runtime/entity/damage/EngineDamageRuntime.h"
#include "runtime/EngineEntityBindings.h"
#include "tracking/EngineHunterTrackingRuntime.h"
#include "tracking/EngineHunterUvShieldRuntime.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>

namespace
{
	constexpr std::uint16_t kInvalidReplicationId = 0xFFFF;
	constexpr float kLocalSearchRadius = 500.0F;
	constexpr std::size_t kHealthCacheCapacity = 64;

	struct ControlObjectCollection
	{
		void **items;
		std::uint32_t count;
		std::uint32_t capacity;
		std::uint32_t flags;
	};

	struct HealthCacheEntry
	{
		const void *entity = nullptr;
		void *module = nullptr;
	};

	struct LocalEntityCache
	{
		const void *entity = nullptr;
		std::uint16_t replicationId = kInvalidReplicationId;
		ULONGLONG updatedAt = 0;
	};

	std::array<HealthCacheEntry, kHealthCacheCapacity> g_healthCache{};
	std::size_t g_healthCacheCursor = 0;
	LocalEntityCache g_localEntityCache{};
	SRWLOCK g_healthCacheLock = SRWLOCK_INIT;
	SRWLOCK g_localEntityCacheLock = SRWLOCK_INIT;

	template <typename Value>
	bool ReadField(const void *object, std::uintptr_t offset, Value &value) noexcept
	{
		value = {};
		if (object == nullptr)
			return false;
		__try
		{
			value = *reinterpret_cast<const Value *>(
				static_cast<const std::uint8_t *>(object) + offset);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			value = {};
			return false;
		}
	}

	bool IsFinite(const DlModVector3V1 &value) noexcept
	{
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			   std::isfinite(value.z);
	}

	/* FindObjectsInRadius allocates its buffer on the game-side UCRT heap. The
	   Debug engine links the Debug CRT, so a plain std::free trips
	   _CrtIsValidHeapPointer. Release through the shared ucrtbase instead. */
	void FreeGameBuffer(void *block) noexcept
	{
		if (block == nullptr)
			return;
		using FreeFunction = void(__cdecl *)(void *);
		static std::atomic<void *> sharedFreeStorage{nullptr};
		void *local = sharedFreeStorage.load(std::memory_order_acquire);
		if (local == nullptr)
		{
			local = reinterpret_cast<void *>(
				GetProcAddress(GetModuleHandleW(L"ucrtbase.dll"), "free"));
			sharedFreeStorage.store(local, std::memory_order_release);
		}
		const auto sharedFree = reinterpret_cast<FreeFunction>(local);
		if (sharedFree != nullptr)
		{
			sharedFree(block);
			return;
		}
		std::free(block);
	}

	DlModVector3V1 DeriveUp(
		const DlModVector3V1 &forward,
		const DlModVector3V1 &up) noexcept
	{
		const float upLength = up.x * up.x + up.y * up.y + up.z * up.z;
		if (IsFinite(up) && upLength > 1.0e-6F)
			return up;

		DlModVector3V1 axis{0.0F, 0.0F, 1.0F};
		if (std::fabs(forward.z) > 0.95F)
			axis = {0.0F, 1.0F, 0.0F};
		const float dot = axis.x * forward.x + axis.y * forward.y +
						  axis.z * forward.z;
		DlModVector3V1 result{
			axis.x - dot * forward.x,
			axis.y - dot * forward.y,
			axis.z - dot * forward.z};
		const float lengthSquared = result.x * result.x +
									result.y * result.y + result.z * result.z;
		if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-6F)
			return axis;
		const float inverseLength = 1.0F / std::sqrt(lengthSquared);
		result.x *= inverseLength;
		result.y *= inverseLength;
		result.z *= inverseLength;
		return result;
	}

	void *AsControlObject(const void *entity) noexcept
	{
		return entity == nullptr
				   ? nullptr
				   : const_cast<std::uint8_t *>(
						 static_cast<const std::uint8_t *>(entity)) +
						 EngineEntityBindings::ControlObjectOffset;
	}

	std::uint8_t ReadModelId(const void *entity) noexcept
	{
		std::uint8_t modelId = 0;
		ReadField(
			AsControlObject(entity),
			EngineEntityBindings::HunterModelIdOffset,
			modelId);
		return modelId;
	}

	bool IsPlayer(const void *entity) noexcept
	{
		return EngineEntityBindings::IsDerivedFrom(entity, "PlayerDI");
	}

	bool IsHunter(const void *entity) noexcept
	{
		return entity != nullptr &&
			   EngineEntityBindings::IsSupportedGame(EngineEntityBindings::GameModule()) &&
			   ReadModelId(entity) == EngineEntityBindings::HunterModelId;
	}

	bool IsHuman(const void *entity) noexcept
	{
		return IsPlayer(entity) && !IsHunter(entity);
	}

	enum EntityClassFlags : std::uint32_t
	{
		EntityClassPlayer = 0x00000001u,
		EntityClassHumanVisual = 0x00000002u,
		EntityClassZombieVisual = 0x00000004u
	};

	struct EntityClassEntry
	{
		std::uintptr_t vtable = 0;
		std::uint32_t flags = 0;
	};

	constexpr std::size_t kEntityClassCacheSize = 512;
	/* 按虚表地址直接映射的缓存：局部性远好于逐个比较的线性表。 */
	thread_local EntityClassEntry g_entityClassCache[kEntityClassCacheSize]{};

	std::uint32_t ClassifyEntityType(const void *entity) noexcept
	{
		std::uintptr_t vtable = 0;
		if (!ReadField(entity, 0, vtable) || vtable == 0)
			return 0;
		EntityClassEntry &cached =
			g_entityClassCache[(vtable >> 4) & (kEntityClassCacheSize - 1)];
		if (cached.vtable == vtable)
			return cached.flags;

		std::uint32_t flags = 0;
		if (EngineEntityBindings::IsDerivedFrom(entity, "PlayerDI"))
		{
			flags |= EntityClassPlayer;
		}
		else if (EngineEntityBindings::IsDerivedFrom(entity, "ZombieAIVis"))
		{
			flags |= EntityClassZombieVisual;
		}
		else if (EngineEntityBindings::IsDerivedFrom(entity, "HumanAIVis"))
		{
			flags |= EntityClassHumanVisual;
		}
		else if (EngineEntityBindings::IsDerivedFrom(entity, "ActorDI"))
		{
			void *visual = nullptr;
			if (ReadField(
					entity, EngineEntityBindings::ActorVisualOffset, visual) &&
				visual != nullptr)
			{
				if (EngineEntityBindings::IsDerivedFrom(visual, "ZombieAIVis"))
					flags |= EntityClassZombieVisual;
				else if (EngineEntityBindings::IsDerivedFrom(visual, "HumanAIVis"))
					flags |= EntityClassHumanVisual;
			}
		}

		g_entityClassCache[(vtable >> 4) & (kEntityClassCacheSize - 1)] =
			{vtable, flags};
		return flags;
	}

	void *FindActiveLevel(const EngineEntityBindings &bindings) noexcept
	{
		const HMODULE game = EngineEntityBindings::GameModule();
		std::uintptr_t gameInstance = 0;
		if (!ReadField(
				reinterpret_cast<const void *>(game),
				EngineEntityBindings::GameGlobalRva,
				gameInstance) ||
			gameInstance == 0)
		{
			return nullptr;
		}
		__try
		{
			return bindings.getActiveLevel(reinterpret_cast<void *>(gameInstance));
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}

	std::uint32_t EntityKindOf(const void *entity) noexcept
	{
		const std::uint32_t flags = ClassifyEntityType(entity);
		if ((flags & EntityClassPlayer) != 0)
		{
			return ReadModelId(entity) == EngineEntityBindings::HunterModelId
					   ? DLMOD_ENTITY_KIND_HUNTER
					   : DLMOD_ENTITY_KIND_HUMAN;
		}
		if ((flags & EntityClassZombieVisual) != 0)
			return DLMOD_ENTITY_KIND_ZOMBIE;
		if ((flags & EntityClassHumanVisual) != 0)
			return DLMOD_ENTITY_KIND_HUMAN;
		return DLMOD_ENTITY_KIND_ENTITY;
	}

	bool ReadReplicationId(
		const EngineEntityBindings &bindings,
		const void *entity,
		std::uint16_t &replicationId) noexcept
	{
		replicationId = kInvalidReplicationId;
		alignas(std::uint64_t) std::uint8_t returnBuffer[16]{};
		__try
		{
			void *idObject = bindings.getObjectId(
				const_cast<void *>(entity), returnBuffer);
			return ReadField(
				idObject, EngineEntityBindings::ReplicationIdOffset, replicationId);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			replicationId = kInvalidReplicationId;
			return false;
		}
	}

	void *FindLocalEntity(const EngineEntityBindings &bindings) noexcept
	{
		const HMODULE game = EngineEntityBindings::GameModule();
		std::uintptr_t gameInstance = 0;
		if (!ReadField(reinterpret_cast<void *>(game), EngineEntityBindings::GameGlobalRva, gameInstance) ||
			gameInstance == 0)
		{
			return nullptr;
		}

		void *level = nullptr;
		std::uint16_t localId = kInvalidReplicationId;
		__try
		{
			level = bindings.getActiveLevel(reinterpret_cast<void *>(gameInstance));
			localId = bindings.getLocalId(reinterpret_cast<void *>(gameInstance));
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
		if (level == nullptr || localId == kInvalidReplicationId)
			return nullptr;

		LocalEntityCache cached{};
		AcquireSRWLockShared(&g_localEntityCacheLock);
		cached = g_localEntityCache;
		ReleaseSRWLockShared(&g_localEntityCacheLock);
		if (cached.entity != nullptr &&
			cached.replicationId == localId &&
			GetTickCount64() - cached.updatedAt <= 1000)
		{
			std::uint16_t cachedId = kInvalidReplicationId;
			if (ReadReplicationId(bindings, cached.entity, cachedId) &&
				cachedId == localId)
			{
				return const_cast<void *>(cached.entity);
			}
		}

		DlModVector3V1 center{};
		void *camera = nullptr;
		__try
		{
			camera = bindings.getActiveCamera(level);
			if (camera != nullptr)
			{
				DlModVector3V1 cameraPosition{};
				if (bindings.getCameraPosition(camera, &cameraPosition) ==
						&cameraPosition &&
					IsFinite(cameraPosition))
				{
					center = cameraPosition;
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			center = {};
		}

		ControlObjectCollection collection{};
		__try
		{
			if (!bindings.findObjects(
					level, &collection, &center, kLocalSearchRadius,
					nullptr, false, nullptr) ||
				collection.items == nullptr)
			{
				FreeGameBuffer(collection.items);
				return nullptr;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			FreeGameBuffer(collection.items);
			return nullptr;
		}
		if (collection.count > collection.capacity || collection.count > 65536)
		{
			FreeGameBuffer(collection.items);
			return nullptr;
		}

		void *result = nullptr;
		for (std::uint32_t index = 0; index < collection.count; ++index)
		{
			void *control = nullptr;
			if (!ReadField(collection.items, index * sizeof(void *), control) ||
				control == nullptr)
			{
				continue;
			}
			auto *entity = static_cast<std::uint8_t *>(control) -
						   EngineEntityBindings::ControlObjectOffset;
			std::uint16_t id = kInvalidReplicationId;
			if (ReadReplicationId(bindings, entity, id) && id == localId)
			{
				result = entity;
				break;
			}
		}
		FreeGameBuffer(collection.items);
		if (result != nullptr)
		{
			AcquireSRWLockExclusive(&g_localEntityCacheLock);
			g_localEntityCache = {result, localId, GetTickCount64()};
			ReleaseSRWLockExclusive(&g_localEntityCacheLock);
		}
		return result;
	}

	void *FindHealthModule(const void *entity) noexcept
	{
		AcquireSRWLockShared(&g_healthCacheLock);
		for (const HealthCacheEntry &entry : g_healthCache)
		{
			if (entry.entity == entity && entry.module != nullptr)
			{
				ReleaseSRWLockShared(&g_healthCacheLock);
				return entry.module;
			}
		}
		ReleaseSRWLockShared(&g_healthCacheLock);

		const HMODULE game = EngineEntityBindings::GameModule();
		if (game == nullptr)
			return nullptr;
		const auto *bytes = static_cast<const std::uint8_t *>(entity);
		void *found = nullptr;
		for (std::uintptr_t offset = sizeof(void *);
			 offset < EngineEntityBindings::ModuleScanSize;
			 offset += sizeof(void *))
		{
			void *candidate = nullptr;
			if (!ReadField(bytes, offset, candidate) || candidate == nullptr)
				continue;
			void *vtable = nullptr;
			if (!ReadField(candidate, 0, vtable))
				continue;
			const auto vtableAddress = reinterpret_cast<std::uintptr_t>(vtable);
			const auto gameBase = reinterpret_cast<std::uintptr_t>(game);
			if (vtableAddress == gameBase + EngineEntityBindings::HealthModuleVtableRva ||
				vtableAddress == gameBase + EngineEntityBindings::BodyPartsHealthModuleVtableRva)
			{
				found = candidate;
				break;
			}
		}
		if (found != nullptr)
		{
			AcquireSRWLockExclusive(&g_healthCacheLock);
			g_healthCache[g_healthCacheCursor] = {entity, found};
			g_healthCacheCursor = (g_healthCacheCursor + 1) % g_healthCache.size();
			ReleaseSRWLockExclusive(&g_healthCacheLock);
		}
		return found;
	}

	bool ReadHealth(const void *entity, float &body, float &bloodMask, std::uint64_t &damager) noexcept
	{
		body = 0.0F;
		bloodMask = 0.0F;
		damager = 0;
		void *module = FindHealthModule(entity);
		return module != nullptr &&
			   ReadField(module, EngineEntityBindings::HealthBodyOffset, body) &&
			   ReadField(module, EngineEntityBindings::HealthBloodMaskOffset, bloodMask) &&
			   ReadField(module, EngineEntityBindings::HealthLastDamagerOffset, damager) &&
			   std::isfinite(body) && std::isfinite(bloodMask);
	}

	/* 只读缓存中的本地实体，不触发新的全场景搜索。 */
	void *LocalEntityCached() noexcept
	{
		LocalEntityCache cached{};
		AcquireSRWLockShared(&g_localEntityCacheLock);
		cached = g_localEntityCache;
		ReleaseSRWLockShared(&g_localEntityCacheLock);
		if (cached.entity != nullptr &&
			GetTickCount64() - cached.updatedAt <= 5000)
		{
			return const_cast<void *>(cached.entity);
		}
		return nullptr;
	}

	bool ReadPosition(
		const EngineEntityBindings &bindings,
		const void *entity,
		DlModVector3V1 &position) noexcept
	{
		position = {};
		DlModVector3V1 value{};
		__try
		{
			if (bindings.getPosition(
					AsControlObject(entity), &value) != &value ||
				!IsFinite(value))
			{
				return false;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
		position = value;
		return true;
	}

	/* CModelObject 校验会遍历 RTTI 层级，骨骼读取每帧都要走一次；
	   按实现对象虚表缓存判定结果，命中时只做一次指针读取。 */
	constexpr std::size_t kModelClassCacheSize = 64;
	struct ModelClassEntry
	{
		std::uintptr_t vtable = 0;
		std::uint8_t isModel = 0;
	};
	thread_local ModelClassEntry g_modelClassCache[kModelClassCacheSize]{};

	bool IsModelClass(const void *modelImpl) noexcept
	{
		std::uintptr_t vtable = 0;
		if (!ReadField(modelImpl, 0, vtable) || vtable == 0)
			return false;
		ModelClassEntry &cached =
			g_modelClassCache[(vtable >> 4) & (kModelClassCacheSize - 1)];
		if (cached.vtable == vtable)
			return cached.isModel != 0;
		const bool isModel =
			EngineEntityBindings::IsDerivedFrom(modelImpl, "CModelObject");
		cached = {vtable, static_cast<std::uint8_t>(isModel ? 1U : 0U)};
		return isModel;
	}

	/* IModelObject 是实体基址上的接口（IControlObject 位于 +0x18），
	   骨骼函数必须以实体基址调用。 */
	bool ReadModelForBones(const void *entity, void *&model) noexcept
	{
		model = nullptr;
		void *modelImpl = nullptr;
		if (entity == nullptr || !ReadField(entity, 0x08, modelImpl) ||
			modelImpl == nullptr || !IsModelClass(modelImpl))
		{
			return false;
		}
		model = const_cast<void *>(entity);
		return true;
	}

	bool ReadBoneJoint(
		const EngineEntityBindings &bindings,
		const void *model,
		std::uint8_t boneId,
		DlModVector3V1 &position) noexcept
	{
		position = {};
		DlModVector3V1 value{};
		void *target = const_cast<void *>(model);
		__try
		{
			if (bindings.resolveBone(target, boneId) < 0 ||
				bindings.getBonePosition(target, &value, boneId) != &value ||
				!IsFinite(value))
			{
				return false;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
		position = value;
		return true;
	}

	bool ResolveBoneIdFromAliases(
		const EngineEntityBindings &bindings,
		const void *model,
		const char *const *aliases,
		std::size_t count,
		std::int32_t &resolved) noexcept
	{
		resolved = -1;
		if (model == nullptr || bindings.getElementOrBoneElementId == nullptr)
			return false;
		__try
		{
			void *target = const_cast<void *>(model);
			for (std::size_t index = 0; index < count; ++index)
			{
				const int boneId = bindings.getElementOrBoneElementId(
					target, aliases[index]);
				if (boneId >= 0 && boneId <= 0xFF)
				{
					resolved = boneId;
					return true;
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			resolved = -1;
		}
		return false;
	}


	bool ReadOrientation(
		const EngineEntityBindings &bindings,
		const void *entity,
		DlModVector3V1 &forward,
		DlModVector3V1 &up) noexcept
	{
		forward = {};
		up = {};
		__try
		{
			if (bindings.getForward(AsControlObject(entity), &forward) != &forward ||
				!IsFinite(forward))
			{
				return false;
			}
			if (bindings.getUp(AsControlObject(entity), &up) != &up ||
				!IsFinite(up))
			{
				up = DeriveUp(forward, {});
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			forward = {};
			up = {};
			return false;
		}
		return true;
	}

	DlModResult ReadEntitySnapshot(
		const EngineEntityBindings &bindings,
		const void *entity,
		DlModEntitySnapshotV1 *snapshot) noexcept
	{
		if (entity == nullptr || snapshot == nullptr ||
			snapshot->structSize < DLMOD_ENTITY_SNAPSHOT_V1_BASE_SIZE ||
			snapshot->abiVersion != MODSDK_ABI_VERSION)
		{
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		DlModEntitySnapshotV1 result{};
		result.structSize = sizeof(result);
		result.abiVersion = MODSDK_ABI_VERSION;
		result.kind = DLMOD_ENTITY_KIND_ENTITY;
		result.controlObject = AsControlObject(entity);
		if (IsHunter(entity))
			result.kind = DLMOD_ENTITY_KIND_HUNTER;
		else if (IsHuman(entity))
			result.kind = DLMOD_ENTITY_KIND_HUMAN;
		else if (IsPlayer(entity))
			result.kind = DLMOD_ENTITY_KIND_PLAYER;

		if (ReadPosition(bindings, entity, result.position))
			result.validFields |= DLMOD_ENTITY_HAS_POSITION;
		if (ReadOrientation(bindings, entity, result.forward, result.up))
			result.validFields |= DLMOD_ENTITY_HAS_ORIENTATION;
		__try
		{
			result.level = bindings.getLevel(const_cast<void *>(entity));
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			result.level = nullptr;
		}
		if (result.level != nullptr)
			result.validFields |= DLMOD_ENTITY_HAS_LEVEL;
		if (ReadReplicationId(bindings, entity, result.replicationId) &&
			result.replicationId != kInvalidReplicationId)
		{
			result.validFields |= DLMOD_ENTITY_HAS_REPLICATION_ID;
		}
		if (ReadHealth(
				entity, result.bodyHealth, result.bloodMaskHealth,
				result.lastDamagerPlayerNetId))
		{
			result.validFields |= DLMOD_ENTITY_HAS_HEALTH;
		}
		result.modelId = ReadModelId(entity);
		const std::size_t copySize = (std::min)(static_cast<std::size_t>(snapshot->structSize), sizeof(result));
		std::memcpy(snapshot, &result, copySize);
		return result.validFields == 0
				   ? DLMOD_RESULT_PARTIAL_SUCCESS
				   : DLMOD_RESULT_SUCCESS;
	}

	bool IsValidHandle(const DlModEntityV1 *entity) noexcept
	{
		return entity != nullptr && entity->instance != nullptr;
	}

	/* 准星拾取：把“枚举 → 分类 → 屏幕预筛 → 骨骼精算”收敛到一次调用。

	   旧路径每次获取目标都会枚举整条街道（含全部静态物体），对每个角色按
	   17 个骨骼别名做字符串查找，再逐个投影；这里改为枚举结果按 200 毫秒
	   缓存复用，只对屏幕中心最近的少数候选读一次骨骼，并把骨骼名解析结果
	   按实体缓存，使稳态开销降到一次矩阵读取 + 少量骨骼读取。 */

	constexpr std::size_t kAimBoneCacheSize = 64;
	constexpr std::size_t kAimActorCacheSize = 256;
	constexpr std::size_t kAimMaxRefine = 8;
	constexpr std::uint32_t kAimDefaultRefine = 4;
	constexpr ULONGLONG kAimActorCacheTtlMs = 200;
	constexpr float kAimActorCacheMoveRadius = 8.0F;
	constexpr float kAimMinimumTargetDistance = 2.0F;
	constexpr float kAimMaximumRadius = 500.0F;

	const char *const kAimHeadAliases[] = {
		"head", "Head", "head_joint", "Head_Joint", "Bip01 Head",
		"bip01_head", "Bip01_Head", "head_end", "Head_End", "neck",
		"Neck", "neck_joint", "Bip01 Neck", "bip01_neck", "Bip01_Neck",
		"neck_end"};
	const char *const kAimBodyAliases[] = {
		"spine", "Spine", "spine1", "Spine1", "spine2", "Spine2",
		"Bip01 Spine", "Bip01 Spine1", "Bip01 Spine2", "bip01_spine",
		"bip01_spine1", "bip01_spine2", "chest", "Chest", "upperchest",
		"torso"};

	struct AimBoneEntry
	{
		const void *entity = nullptr;
		std::int32_t headBone = -1;
		std::int32_t bodyBone = -1;
		/* 已尝试解析但失败的槽位，避免每帧重复做字符串查找。 */
		std::uint8_t tried = 0;
	};
	thread_local AimBoneEntry g_aimBoneCache[kAimBoneCacheSize]{};
	thread_local std::size_t g_aimBoneCursor = 0;

	struct AimActorCache
	{
		void *level = nullptr;
		DlModVector3V1 center{};
		float radius = 0.0F;
		ULONGLONG updatedAt = 0;
		std::uint32_t count = 0;
		std::uint32_t scanned = 0;
		/* 记录已分类的角色指针与种类，复用缓存时不再触碰 RTTI。 */
		const void *entities[kAimActorCacheSize]{};
		std::uint32_t kinds[kAimActorCacheSize]{};
	};
	thread_local AimActorCache g_aimActorCache{};

	struct AimCameraSnapshot
	{
		DlModVector3V1 position{};
		float matrix[16]{};
	};

	struct AimCandidate
	{
		const void *entity = nullptr;
		DlModVector3V1 position{};
		float screenX = 0.0F;
		float screenY = 0.0F;
		float screenDistance = 0.0F;
		float distance = 0.0F;
		std::uint32_t kind = 0;
	};

	bool IsActorKind(std::uint32_t kind) noexcept
	{
		return kind == DLMOD_ENTITY_KIND_PLAYER ||
			   kind == DLMOD_ENTITY_KIND_HUMAN ||
			   kind == DLMOD_ENTITY_KIND_HUNTER ||
			   kind == DLMOD_ENTITY_KIND_ZOMBIE;
	}

	bool ReadAimCamera(
		const EngineEntityBindings &bindings,
		void *level,
		AimCameraSnapshot &snapshot) noexcept
	{
		snapshot = {};
		if (bindings.getActiveCamera == nullptr ||
			bindings.getCameraPosition == nullptr ||
			bindings.getCameraMatrix == nullptr)
		{
			return false;
		}
		__try
		{
			void *camera = bindings.getActiveCamera(level);
			if (camera == nullptr)
				return false;
			if (bindings.getCameraPosition(camera, &snapshot.position) !=
					&snapshot.position ||
				!IsFinite(snapshot.position))
			{
				return false;
			}
			const float *source = bindings.getCameraMatrix(camera);
			if (source == nullptr)
				return false;
			for (std::size_t index = 0; index < 16; ++index)
			{
				if (!std::isfinite(source[index]))
					return false;
				snapshot.matrix[index] = source[index];
			}
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			snapshot = {};
			return false;
		}
	}

	bool ProjectWithMatrix(
		const float (&matrix)[16],
		const DlModVector3V1 &world,
		float width,
		float height,
		float &screenX,
		float &screenY) noexcept
	{
		const float clipX = world.x * matrix[0] + world.y * matrix[1] +
							world.z * matrix[2] + matrix[3];
		const float clipY = world.x * matrix[4] + world.y * matrix[5] +
							world.z * matrix[6] + matrix[7];
		const float clipW = world.x * matrix[12] + world.y * matrix[13] +
							world.z * matrix[14] + matrix[15];
		if (!std::isfinite(clipX) || !std::isfinite(clipY) ||
			!std::isfinite(clipW) || clipW < 0.001F)
		{
			return false;
		}
		const float x = (clipX / clipW + 1.0F) * width * 0.5F;
		const float y = (1.0F - clipY / clipW) * height * 0.5F;
		if (!std::isfinite(x) || !std::isfinite(y))
			return false;
		screenX = x;
		screenY = y;
		return true;
	}

	AimBoneEntry &AimBoneSlot(const void *entity) noexcept
	{
		for (AimBoneEntry &entry : g_aimBoneCache)
		{
			if (entry.entity == entity)
				return entry;
		}
		AimBoneEntry &slot = g_aimBoneCache[g_aimBoneCursor];
		g_aimBoneCursor = (g_aimBoneCursor + 1) % kAimBoneCacheSize;
		slot.entity = entity;
		slot.headBone = -1;
		slot.bodyBone = -1;
		slot.tried = 0;
		return slot;
	}

	bool EnsureAimBoneId(
		const EngineEntityBindings &bindings,
		const void *entity,
		bool head,
		std::int32_t &boneId) noexcept
	{
		AimBoneEntry &entry = AimBoneSlot(entity);
		const std::uint8_t triedMask =
			static_cast<std::uint8_t>(head ? 0x01U : 0x02U);
		std::int32_t &cached = head ? entry.headBone : entry.bodyBone;
		if (cached >= 0)
		{
			boneId = cached;
			return true;
		}
		if ((entry.tried & triedMask) != 0)
			return false;
		entry.tried = static_cast<std::uint8_t>(entry.tried | triedMask);
		void *model = nullptr;
		if (!ReadModelForBones(entity, model))
			return false;
		const bool resolved = head
			? ResolveBoneIdFromAliases(
				  bindings, model, kAimHeadAliases,
				  std::size(kAimHeadAliases), cached)
			: ResolveBoneIdFromAliases(
				  bindings, model, kAimBodyAliases,
				  std::size(kAimBodyAliases), cached);
		if (!resolved)
			return false;
		boneId = cached;
		return true;
	}

	void InsertAimCandidate(
		AimCandidate (&top)[kAimMaxRefine],
		std::size_t &count,
		std::size_t limit,
		const AimCandidate &candidate) noexcept
	{
		if (count >= limit && candidate.screenDistance >= top[count - 1].screenDistance)
			return;
		std::size_t slot = count < limit ? count++ : count - 1;
		while (slot > 0 && top[slot - 1].screenDistance > candidate.screenDistance)
		{
			top[slot] = top[slot - 1];
			--slot;
		}
		top[slot] = candidate;
	}

	bool RefreshAimActors(
		const EngineEntityBindings &bindings,
		void *level,
		const DlModVector3V1 &center,
		float radius,
		const void *localEntity,
		std::uint32_t &scanned) noexcept
	{
		AimActorCache &cache = g_aimActorCache;
		cache.level = level;
		cache.center = center;
		cache.radius = radius;
		cache.updatedAt = GetTickCount64();
		cache.count = 0;
		cache.scanned = 0;
		scanned = 0;

		ControlObjectCollection collection{};
		bool queried = false;
		__try
		{
			queried = bindings.findObjects(
				level, &collection, &center, radius, nullptr, false, nullptr);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			queried = false;
			collection = {};
		}
		if (!queried || collection.items == nullptr)
		{
			FreeGameBuffer(collection.items);
			return false;
		}
		if (collection.count > 65536 || collection.count > collection.capacity)
		{
			FreeGameBuffer(collection.items);
			return false;
		}

		for (std::uint32_t index = 0;
			 index < collection.count && cache.count < kAimActorCacheSize;
			 ++index)
		{
			void *control = nullptr;
			if (!ReadField(
					collection.items, index * sizeof(void *), control) ||
				control == nullptr)
			{
				continue;
			}
			++scanned;
			const void *entity = static_cast<std::uint8_t *>(control) -
								 EngineEntityBindings::ControlObjectOffset;
			if (entity == localEntity)
				continue;
			const std::uint32_t kind = EntityKindOf(entity);
			if (!IsActorKind(kind))
				continue;
			cache.entities[cache.count] = entity;
			cache.kinds[cache.count] = kind;
			++cache.count;
		}
		FreeGameBuffer(collection.items);
		cache.scanned = scanned;
		return true;
	}

}

const DlModEntityApiV1 *EngineEntityRuntime::GetApi() noexcept
{
	static const DlModEntityApiV1 api = {
		sizeof(DlModEntityApiV1),
		MODSDK_ABI_VERSION,
		&GetLocalEntity,
		&GetLocalPlayer,
		&GetLocalHuman,
		&GetCurrentHunter,
		&ReadEntity,
		&ReadPlayer,
		&ReadHuman,
		&ReadHunter,
		&SetPosition,
		&SetOrientation,
		&ReadBonePosition,
		&ReadHunterMotion,
		&InstallHunterTracking,
		&IsHunterTrackingInstalled,
		&EnumerateNearby,
		&ReadBoneByName,
		&InstallUvShieldTracking,
		&IsUvShieldTrackingInstalled,
		&ReadHunterUvShield,
		&SetHunterUvShield,
		&SetHunterUvShieldCharges,
		&SetHunterUvShieldDuration,
		&SetHunterUvShieldProgress,
		&RegisterHunterUvShieldHooks,
		&UnregisterHunterUvShieldHooks,
		&InstallDamageTracking,
		&IsDamageTrackingInstalled,
		&RegisterDamageHook,
		&UnregisterDamageHook,
		&PickAimTarget,
		&ResolveBoneId,
		&ReadBoneById};
#if defined(_M_X64)
	return EngineEntityBindings::Resolve() != nullptr ? &api : nullptr;
#else
	return nullptr;
#endif
}

void EngineEntityRuntime::Shutdown() noexcept
{
	EngineDamageRuntime::Shutdown();
	EngineHunterUvShieldRuntime::Shutdown();
	EngineHunterTrackingRuntime::Shutdown();
}

void *EngineEntityRuntime::LocalEntityInstance() noexcept
{
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	return bindings == nullptr ? nullptr : FindLocalEntity(*bindings);
}

bool EngineEntityRuntime::ReadEntityHealth(
	const void *entity,
	float &bodyHealth) noexcept
{
	bodyHealth = 0.0F;
	float bloodMask = 0.0F;
	std::uint64_t damager = 0;
	return entity != nullptr &&
		   ReadHealth(entity, bodyHealth, bloodMask, damager);
}

bool EngineEntityRuntime::ReadEntityHealthDetails(
	const void *entity,
	float &bodyHealth,
	float &bloodMaskHealth,
	std::uint64_t &lastDamagerPlayerNetId) noexcept
{
	return entity != nullptr &&
		   ReadHealth(entity, bodyHealth, bloodMaskHealth, lastDamagerPlayerNetId);
}

bool EngineEntityRuntime::IsPlayerInstance(const void *entity) noexcept
{
	return IsPlayer(entity);
}

bool EngineEntityRuntime::IsHunterInstance(const void *entity) noexcept
{
	return IsHunter(entity);
}

bool EngineEntityRuntime::IsHumanPlayerInstance(const void *entity) noexcept
{
	return IsHuman(entity);
}

bool EngineEntityRuntime::IsHumanVisualInstance(const void *entity) noexcept
{
	return (ClassifyEntityType(entity) & EntityClassHumanVisual) != 0;
}

bool EngineEntityRuntime::IsZombieInstance(const void *entity) noexcept
{
	return (ClassifyEntityType(entity) & EntityClassZombieVisual) != 0;
}

std::uint8_t EngineEntityRuntime::EntityModelId(const void *entity) noexcept
{
	std::uint8_t modelId = 0;
	ReadField(AsControlObject(entity), EngineEntityBindings::HunterModelIdOffset, modelId);
	return modelId;
}

DlModEntityKindV1 EngineEntityRuntime::EntityKind(const void *entity) noexcept
{
	return static_cast<DlModEntityKindV1>(EntityKindOf(entity));
}

std::uint32_t EngineEntityRuntime::EntityClassFlags(const void *entity) noexcept
{
	return ClassifyEntityType(entity);
}

DlModResult EngineEntityRuntime::ReadSnapshotForInstance(
	const void *entity,
	DlModEntitySnapshotV1 *snapshot) noexcept
{
	if (entity == nullptr || snapshot == nullptr ||
		snapshot->structSize < DLMOD_ENTITY_SNAPSHOT_V1_BASE_SIZE ||
		snapshot->abiVersion != MODSDK_ABI_VERSION)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	return bindings == nullptr
			   ? DLMOD_RESULT_UNSUPPORTED_FEATURE
			   : ReadEntitySnapshot(*bindings, entity, snapshot);
}

DlModResult EngineEntityRuntime::ReadPlayerSnapshot(
	const void *entity,
	DlModPlayerSnapshotV1 *snapshot) noexcept
{
	if (entity == nullptr || snapshot == nullptr ||
		snapshot->structSize < DLMOD_PLAYER_SNAPSHOT_V1_BASE_SIZE ||
		snapshot->abiVersion != MODSDK_ABI_VERSION)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	if (!IsPlayer(entity))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	const std::uint32_t size = snapshot->structSize;
	const DlModResult result = ReadSnapshotForInstance(
		entity, reinterpret_cast<DlModEntitySnapshotV1 *>(snapshot));
	snapshot->playerFlags = 0;
	snapshot->structSize = size;
	return result;
}

DlModResult EngineEntityRuntime::ReadHumanSnapshot(
	const void *entity,
	DlModHumanSnapshotV1 *snapshot) noexcept
{
	if (entity == nullptr || snapshot == nullptr ||
		snapshot->structSize < DLMOD_HUMAN_SNAPSHOT_V1_BASE_SIZE ||
		snapshot->abiVersion != MODSDK_ABI_VERSION)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	if (!IsHuman(entity))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	const std::uint32_t size = snapshot->structSize;
	const DlModResult result = ReadSnapshotForInstance(
		entity, reinterpret_cast<DlModEntitySnapshotV1 *>(snapshot));
	snapshot->playerFlags = 0;
	snapshot->humanFlags = 0;
	snapshot->structSize = size;
	return result;
}

DlModResult EngineEntityRuntime::ReadHunterSnapshot(
	const void *entity,
	DlModHunterSnapshotV1 *snapshot) noexcept
{
	if (entity == nullptr || snapshot == nullptr ||
		snapshot->structSize < DLMOD_HUNTER_SNAPSHOT_V1_BASE_SIZE ||
		snapshot->abiVersion != MODSDK_ABI_VERSION)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	if (!IsHunter(entity))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	const std::uint32_t size = snapshot->structSize;
	const DlModResult result = ReadSnapshotForInstance(
		entity, reinterpret_cast<DlModEntitySnapshotV1 *>(snapshot));
	snapshot->playerFlags = 0;
	EngineHunterTrackingRuntime::ReadMotion(
		AsControlObject(entity), snapshot->motion);
	snapshot->structSize = size;
	return result;
}

DlModResult EngineEntityRuntime::SetEntityPosition(
	const void *entity,
	const DlModVector3V1 *position) noexcept
{
	DlModEntityV1 handle{const_cast<void *>(entity)};
	return SetPosition(&handle, position);
}

DlModResult EngineEntityRuntime::SetEntityOrientation(
	const void *entity,
	const DlModVector3V1 *forward,
	const DlModVector3V1 *up) noexcept
{
	DlModEntityV1 handle{const_cast<void *>(entity)};
	return SetOrientation(&handle, forward, up);
}

DlModResult EngineEntityRuntime::ReadEntityBonePosition(
	const void *entity,
	std::uint8_t boneId,
	DlModVector3V1 *position) noexcept
{
	DlModHunterV1 handle{};
	handle.instance = const_cast<void *>(entity);
	return ReadBonePosition(&handle, boneId, position);
}

void *EngineEntityRuntime::ControlObjectForInstance(const void *entity) noexcept
{
	return AsControlObject(entity);
}

bool EngineEntityRuntime::ReadPositionForInstance(
	const void *entity,
	DlModVector3V1 &position) noexcept
{
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	return bindings != nullptr && ReadPosition(*bindings, entity, position);
}

bool EngineEntityRuntime::ReadOrientationForInstance(
	const void *entity,
	DlModVector3V1 &forward,
	DlModVector3V1 &up) noexcept
{
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	return bindings != nullptr && ReadOrientation(*bindings, entity, forward, up);
}

bool EngineEntityRuntime::ReadModelObjectForInstance(
	const void *entity,
	void *&model) noexcept
{
	model = nullptr;
	if (entity == nullptr)
		return false;
	return ReadField(entity, 0x08, model) && model != nullptr &&
		   EngineEntityBindings::IsDerivedFrom(model, "CModelObject");
}

DlModResult MODSDK_CALL EngineEntityRuntime::GetLocalEntity(DlModEntityV1 *entity) noexcept
{
	if (entity == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	entity->instance = nullptr;
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr)
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	entity->instance = FindLocalEntity(*bindings);
	return entity->instance != nullptr ? DLMOD_RESULT_SUCCESS : DLMOD_RESULT_NOT_FOUND;
}

DlModResult MODSDK_CALL EngineEntityRuntime::GetLocalPlayer(DlModPlayerV1 *player) noexcept
{
	if (player == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	player->instance = nullptr;
	DlModEntityV1 entity{};
	const DlModResult result = GetLocalEntity(&entity);
	if (result != DLMOD_RESULT_SUCCESS)
		return result;
	if (!IsPlayer(entity.instance))
		return DLMOD_RESULT_NOT_FOUND;
	player->instance = entity.instance;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::GetLocalHuman(DlModHumanV1 *human) noexcept
{
	if (human == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	human->instance = nullptr;
	DlModPlayerV1 player{};
	const DlModResult result = GetLocalPlayer(&player);
	if (result != DLMOD_RESULT_SUCCESS)
		return result;
	if (!IsHuman(player.instance))
		return DLMOD_RESULT_NOT_FOUND;
	human->instance = player.instance;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::GetCurrentHunter(DlModHunterV1 *hunter) noexcept
{
	if (hunter == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	hunter->instance = nullptr;
	void *control = EngineHunterTrackingRuntime::GetCurrentControlObject();
	if (control == nullptr ||
		!IsHunter(static_cast<std::uint8_t *>(control) -
				  EngineEntityBindings::ControlObjectOffset))
	{
		return DLMOD_RESULT_NOT_FOUND;
	}
	hunter->instance = static_cast<std::uint8_t *>(control) -
					   EngineEntityBindings::ControlObjectOffset;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadEntity(
	const DlModEntityV1 *entity,
	DlModEntitySnapshotV1 *snapshot) noexcept
{
	if (!IsValidHandle(entity))
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	return bindings == nullptr
			   ? DLMOD_RESULT_UNSUPPORTED_FEATURE
			   : ReadEntitySnapshot(*bindings, entity->instance, snapshot);
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadPlayer(
	const DlModPlayerV1 *player,
	DlModPlayerSnapshotV1 *snapshot) noexcept
{
	if (player == nullptr ||
		!IsValidHandle(reinterpret_cast<const DlModEntityV1 *>(player)) ||
		snapshot == nullptr ||
		snapshot->structSize < DLMOD_PLAYER_SNAPSHOT_V1_BASE_SIZE ||
		snapshot->abiVersion != MODSDK_ABI_VERSION)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (!IsPlayer(player->instance))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	const std::uint32_t size = snapshot->structSize;
	const DlModResult result = ReadEntity(
		reinterpret_cast<const DlModEntityV1 *>(player),
		reinterpret_cast<DlModEntitySnapshotV1 *>(snapshot));
	snapshot->playerFlags = 0;
	snapshot->structSize = size;
	return result;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadHuman(
	const DlModHumanV1 *human,
	DlModHumanSnapshotV1 *snapshot) noexcept
{
	if (human == nullptr || !IsValidHandle(reinterpret_cast<const DlModEntityV1 *>(human)) ||
		snapshot == nullptr ||
		snapshot->structSize < DLMOD_HUMAN_SNAPSHOT_V1_BASE_SIZE ||
		snapshot->abiVersion != MODSDK_ABI_VERSION)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (!IsHuman(human->instance))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	const std::uint32_t size = snapshot->structSize;
	const DlModResult result = ReadEntity(
		reinterpret_cast<const DlModEntityV1 *>(human),
		reinterpret_cast<DlModEntitySnapshotV1 *>(snapshot));
	snapshot->playerFlags = 0;
	snapshot->humanFlags = 0;
	snapshot->structSize = size;
	return result;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadHunter(
	const DlModHunterV1 *hunter,
	DlModHunterSnapshotV1 *snapshot) noexcept
{
	if (hunter == nullptr || !IsValidHandle(reinterpret_cast<const DlModEntityV1 *>(hunter)) ||
		snapshot == nullptr ||
		snapshot->structSize < DLMOD_HUNTER_SNAPSHOT_V1_BASE_SIZE ||
		snapshot->abiVersion != MODSDK_ABI_VERSION)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (!IsHunter(hunter->instance))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	const std::uint32_t size = snapshot->structSize;
	const DlModResult result = ReadEntity(
		reinterpret_cast<const DlModEntityV1 *>(hunter),
		reinterpret_cast<DlModEntitySnapshotV1 *>(snapshot));
	snapshot->playerFlags = 0;
	EngineHunterTrackingRuntime::ReadMotion(
		AsControlObject(hunter->instance), snapshot->motion);
	snapshot->structSize = size;
	return result;
}

DlModResult MODSDK_CALL EngineEntityRuntime::SetPosition(
	const DlModEntityV1 *entity,
	const DlModVector3V1 *position) noexcept
{
	if (!IsValidHandle(entity) || position == nullptr || !IsFinite(*position))
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr)
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	__try
	{
		bindings->setPosition(AsControlObject(entity->instance), position);
		return DLMOD_RESULT_SUCCESS;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return DLMOD_RESULT_INVALID_STATE;
	}
}

DlModResult MODSDK_CALL EngineEntityRuntime::SetOrientation(
	const DlModEntityV1 *entity,
	const DlModVector3V1 *forward,
	const DlModVector3V1 *up) noexcept
{
	if (!IsValidHandle(entity) || forward == nullptr || up == nullptr ||
		!IsFinite(*forward) || !IsFinite(*up))
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr)
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	DlModVector3V1 position{};
	if (!ReadPosition(*bindings, entity->instance, position))
		return DLMOD_RESULT_INVALID_STATE;
	const DlModVector3V1 stableUp = DeriveUp(*forward, *up);
	__try
	{
		bindings->setOrientation(
			AsControlObject(entity->instance), &stableUp, forward, &position);
		return DLMOD_RESULT_SUCCESS;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return DLMOD_RESULT_INVALID_STATE;
	}
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadBonePosition(
	const DlModHunterV1 *hunter,
	uint8_t boneId,
	DlModVector3V1 *position) noexcept
{
	if (hunter == nullptr || !IsValidHandle(reinterpret_cast<const DlModEntityV1 *>(hunter)) ||
		position == nullptr)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	*position = {};
	if (!IsHunter(hunter->instance))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr || bindings->getBonePosition == nullptr ||
		bindings->resolveBone == nullptr)
	{
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}
	/* IModelObject 是实体基址上的接口（IControlObject 位于 +0x18），
	   骨骼函数必须以实体基址调用，否则会取到控制对象的实现指针。 */
	void *model = hunter->instance;
	void *modelImpl = nullptr;
	if (!ReadField(model, 0x08, modelImpl) || modelImpl == nullptr ||
		!EngineEntityBindings::IsDerivedFrom(modelImpl, "CModelObject"))
	{
		return DLMOD_RESULT_NOT_FOUND;
	}
	DlModVector3V1 value{};
	__try
	{
		if (bindings->resolveBone(model, boneId) < 0 ||
			bindings->getBonePosition(model, &value, boneId) != &value ||
			!IsFinite(value))
		{
			return DLMOD_RESULT_NOT_FOUND;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return DLMOD_RESULT_INVALID_STATE;
	}
	*position = value;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadHunterMotion(
	const DlModHunterV1 *hunter,
	DlModHunterMotionV1 *motion) noexcept
{
	if (hunter == nullptr || !IsValidHandle(reinterpret_cast<const DlModEntityV1 *>(hunter)) ||
		motion == nullptr || motion->structSize < sizeof(DlModHunterMotionV1) ||
		motion->abiVersion != MODSDK_ABI_VERSION)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (!IsHunter(hunter->instance))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	DlModHunterMotionV1 result{};
	if (!EngineHunterTrackingRuntime::ReadMotion(
			AsControlObject(hunter->instance), result))
		return DLMOD_RESULT_NOT_FOUND;
	*motion = result;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::InstallHunterTracking() noexcept
{
	if (!EngineHunterTrackingRuntime::Install())
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	/* UV shield hooks belong to the hunter feature set; failure is non-fatal. */
	EngineHunterUvShieldRuntime::Install();
	return DLMOD_RESULT_SUCCESS;
}

uint8_t MODSDK_CALL EngineEntityRuntime::IsHunterTrackingInstalled() noexcept
{
	return EngineHunterTrackingRuntime::IsInstalled() ? 1U : 0U;
}

DlModResult MODSDK_CALL EngineEntityRuntime::InstallUvShieldTracking() noexcept
{
	return EngineHunterUvShieldRuntime::Install()
			   ? DLMOD_RESULT_SUCCESS
			   : DLMOD_RESULT_UNSUPPORTED_FEATURE;
}

uint8_t MODSDK_CALL EngineEntityRuntime::IsUvShieldTrackingInstalled() noexcept
{
	return EngineHunterUvShieldRuntime::IsInstalled() ? 1U : 0U;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadHunterUvShield(
	DlModHunterUvShieldV1 *snapshot) noexcept
{
	if (snapshot == nullptr ||
		snapshot->structSize < sizeof(DlModHunterUvShieldV1) ||
		snapshot->abiVersion != MODSDK_ABI_VERSION)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	DlModHunterUvShieldV1 result{};
	if (!EngineHunterUvShieldRuntime::Read(result))
		return DLMOD_RESULT_NOT_FOUND;
	const std::uint32_t size = snapshot->structSize;
	*snapshot = result;
	snapshot->structSize = size;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::SetHunterUvShield(
	uint32_t mode,
	float value) noexcept
{
	return EngineHunterUvShieldRuntime::SetOverride(mode, value);
}

DlModResult MODSDK_CALL EngineEntityRuntime::SetHunterUvShieldCharges(
	int32_t charges) noexcept
{
	return EngineHunterUvShieldRuntime::SetChargeOverride(charges);
}

DlModResult MODSDK_CALL EngineEntityRuntime::SetHunterUvShieldDuration(
	float seconds) noexcept
{
	return EngineHunterUvShieldRuntime::SetDurationOverride(seconds);
}

DlModResult MODSDK_CALL EngineEntityRuntime::SetHunterUvShieldProgress(
	float ratio) noexcept
{
	return EngineHunterUvShieldRuntime::SetProgressRatio(ratio);
}

DlModResult MODSDK_CALL EngineEntityRuntime::RegisterHunterUvShieldHooks(
	const DlModHunterUvShieldHookDescV1 *desc) noexcept
{
	if (desc == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	return EngineHunterUvShieldRuntime::RegisterHooks(*desc);
}

DlModResult MODSDK_CALL EngineEntityRuntime::UnregisterHunterUvShieldHooks() noexcept
{
	return EngineHunterUvShieldRuntime::UnregisterHooks();
}

DlModResult MODSDK_CALL EngineEntityRuntime::InstallDamageTracking() noexcept
{
	return EngineDamageRuntime::Install()
			   ? DLMOD_RESULT_SUCCESS
			   : DLMOD_RESULT_UNSUPPORTED_FEATURE;
}

uint8_t MODSDK_CALL EngineEntityRuntime::IsDamageTrackingInstalled() noexcept
{
	return EngineDamageRuntime::IsInstalled() ? 1U : 0U;
}

DlModResult MODSDK_CALL EngineEntityRuntime::RegisterDamageHook(
	ModEngineDamagePreHookFn pre,
	ModEngineDamagePostHookFn post) noexcept
{
	return EngineDamageRuntime::RegisterHook(pre, post);
}

DlModResult MODSDK_CALL EngineEntityRuntime::UnregisterDamageHook(
	ModEngineDamagePreHookFn pre,
	ModEngineDamagePostHookFn post) noexcept
{
	return EngineDamageRuntime::UnregisterHook(pre, post);
}

DlModResult MODSDK_CALL EngineEntityRuntime::EnumerateNearby(
	const DlModVector3V1 *center,
	float radius,
	DlModNearbyEntityV1 *entities,
	uint32_t capacity,
	uint32_t *count) noexcept
{
	if (center == nullptr || entities == nullptr || count == nullptr ||
		capacity == 0 || !IsFinite(*center) || !std::isfinite(radius) ||
		radius <= 0.0F || radius > 500.0F)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	*count = 0;
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr)
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	void *level = FindActiveLevel(*bindings);
	if (level == nullptr)
		return DLMOD_RESULT_NOT_FOUND;

	ControlObjectCollection collection{};
	bool queried = false;
	__try
	{
		queried = bindings->findObjects(
			level, &collection, center, radius, nullptr, false, nullptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		queried = false;
		collection = {};
	}
	if (!queried || collection.items == nullptr)
	{
		FreeGameBuffer(collection.items);
		return DLMOD_RESULT_NOT_FOUND;
	}
	if (collection.count > 65536 || collection.count > collection.capacity)
	{
		FreeGameBuffer(collection.items);
		return DLMOD_RESULT_INVALID_STATE;
	}

	std::uint32_t written = 0;
	/* 单次扫描：角色从头部写入，普通对象从尾部倒着写入，
	   既保证角色优先，也避免为补静态物体再扫一遍整个缓冲区。 */
	std::uint32_t back = capacity;
	for (std::uint32_t index = 0;
		 index < collection.count && written < back;
		 ++index)
	{
		void *control = nullptr;
		if (!ReadField(
				collection.items, index * sizeof(void *), control) ||
			control == nullptr)
		{
			continue;
		}
		auto *entity = static_cast<std::uint8_t *>(control) -
					   EngineEntityBindings::ControlObjectOffset;
		const std::uint32_t kind = EntityKindOf(entity);
		DlModNearbyEntityV1 item{};
		item.structSize = sizeof(DlModNearbyEntityV1);
		item.kind = kind;
		item.identity = static_cast<std::uint64_t>(
			reinterpret_cast<std::uintptr_t>(entity));
		item.entity.instance = entity;
		DlModVector3V1 position{};
		if (ReadPosition(*bindings, entity, position))
		{
			item.position = position;
			item.validFields |= DLMOD_ENTITY_HAS_POSITION;
		}
		if (IsActorKind(kind))
		{
			entities[written] = item;
			++written;
		}
		else
		{
			entities[--back] = item;
		}
	}
	/* 把尾部暂存的普通对象搬到角色之后。 */
	const std::uint32_t statics = capacity - back;
	if (statics > 0)
	{
		std::memmove(
			entities + written, entities + back,
			static_cast<std::size_t>(statics) * sizeof(DlModNearbyEntityV1));
	}
	written += statics;
	FreeGameBuffer(collection.items);
	*count = written;
	return written < collection.count
			   ? DLMOD_RESULT_PARTIAL_SUCCESS
			   : DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadBoneByName(
	const DlModEntityV1 *entity,
	const char *name,
	DlModVector3V1 *position) noexcept
{
	if (!IsValidHandle(entity) || name == nullptr || position == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	*position = {};
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr || bindings->getElementOrBoneElementId == nullptr ||
		bindings->resolveBone == nullptr ||
		bindings->getBonePosition == nullptr)
	{
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}
	void *model = nullptr;
	if (!ReadModelForBones(entity->instance, model))
		return DLMOD_RESULT_NOT_FOUND;
	std::int32_t boneId = -1;
	if (!ResolveBoneIdFromAliases(*bindings, model, &name, 1, boneId))
		return DLMOD_RESULT_NOT_FOUND;
	DlModVector3V1 value{};
	if (!ReadBoneJoint(
			*bindings, model, static_cast<std::uint8_t>(boneId), value))
	{
		return DLMOD_RESULT_NOT_FOUND;
	}
	*position = value;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ResolveBoneId(
	const DlModEntityV1 *entity,
	const char *name,
	uint8_t *boneId) noexcept
{
	if (!IsValidHandle(entity) || name == nullptr || boneId == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	*boneId = 0;
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr || bindings->getElementOrBoneElementId == nullptr)
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	void *model = nullptr;
	if (!ReadModelForBones(entity->instance, model))
		return DLMOD_RESULT_NOT_FOUND;
	std::int32_t resolved = -1;
	if (!ResolveBoneIdFromAliases(*bindings, model, &name, 1, resolved))
		return DLMOD_RESULT_NOT_FOUND;
	*boneId = static_cast<uint8_t>(resolved);
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadBoneById(
	const DlModEntityV1 *entity,
	uint8_t boneId,
	DlModVector3V1 *position) noexcept
{
	if (!IsValidHandle(entity) || position == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	*position = {};
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr || bindings->resolveBone == nullptr ||
		bindings->getBonePosition == nullptr)
	{
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}
	void *model = nullptr;
	if (!ReadModelForBones(entity->instance, model))
		return DLMOD_RESULT_NOT_FOUND;
	DlModVector3V1 value{};
	if (!ReadBoneJoint(*bindings, model, boneId, value))
		return DLMOD_RESULT_NOT_FOUND;
	*position = value;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::PickAimTarget(
	const DlModAimPickOptionsV1 *options,
	DlModAimPickResultV1 *result) noexcept
{
	if (options == nullptr || result == nullptr ||
		options->structSize < DLMOD_AIM_PICK_OPTIONS_V1_SIZE ||
		result->structSize < DLMOD_AIM_PICK_RESULT_V1_SIZE ||
		options->abiVersion != MODSDK_ABI_VERSION)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (!std::isfinite(options->viewportWidth) ||
		!std::isfinite(options->viewportHeight) ||
		options->viewportWidth <= 0.0F || options->viewportHeight <= 0.0F ||
		!std::isfinite(options->radius) || options->radius <= 0.0F)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	DlModAimPickResultV1 picked{};
	picked.structSize = DLMOD_AIM_PICK_RESULT_V1_SIZE;
	picked.abiVersion = MODSDK_ABI_VERSION;
	*result = picked;

	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr)
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	void *level = FindActiveLevel(*bindings);
	if (level == nullptr)
		return DLMOD_RESULT_NOT_FOUND;

	AimCameraSnapshot camera{};
	if (!ReadAimCamera(*bindings, level, camera))
		return DLMOD_RESULT_NOT_FOUND;

	const float radius = (std::min)(options->radius, kAimMaximumRadius);
	const float width = options->viewportWidth;
	const float height = options->viewportHeight;
	/* center 为零向量时以相机位置为球心/评分原点。 */
	DlModVector3V1 center = options->center;
	if (!IsFinite(center) ||
		(center.x == 0.0F && center.y == 0.0F && center.z == 0.0F))
	{
		center = camera.position;
	}
	const float centerX = width * 0.5F;
	const float centerY = height * 0.5F;
	const float screenScale = width * width + height * height;
	const float fovRadius = std::isfinite(options->fovRadius)
								? options->fovRadius
								: 0.0F;
	const float fovRadiusSquared = fovRadius * fovRadius;
	const float maximumDistanceSquared = radius * radius;
	const float minimumDistanceSquared =
		kAimMinimumTargetDistance * kAimMinimumTargetDistance;
	const std::uint32_t allowedKinds = options->allowedKinds != 0
										   ? options->allowedKinds
										   : DLMOD_AIM_KIND_DEFAULT;
	const bool head = options->boneTarget == DLMOD_AIM_BONE_HEAD;
	std::size_t refineLimit = options->maxBoneReads;
	if (refineLimit == 0)
		refineLimit = kAimDefaultRefine;
	refineLimit = (std::min)(refineLimit, kAimMaxRefine);

	/* 角色列表按 200 毫秒 / 8 米位移失效；期间只做位置读取与投影。 */
	AimActorCache &cache = g_aimActorCache;
	const std::uint64_t now = GetTickCount64();
	const float offsetX = center.x - cache.center.x;
	const float offsetY = center.y - cache.center.y;
	const float offsetZ = center.z - cache.center.z;
	const bool cacheFresh =
		cache.level == level && cache.radius == radius &&
		now - cache.updatedAt <= kAimActorCacheTtlMs &&
		offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ <=
			kAimActorCacheMoveRadius * kAimActorCacheMoveRadius;

	std::uint32_t scanned = 0;
	bool reused = cacheFresh;
	if (!cacheFresh)
	{
		const void *localEntity = LocalEntityCached();
		if (localEntity == nullptr)
			localEntity = FindLocalEntity(*bindings);
		RefreshAimActors(
			*bindings, level, center, radius, localEntity, scanned);
		reused = false;
	}
	else
	{
		scanned = cache.scanned;
	}

	AimCandidate top[kAimMaxRefine]{};
	std::size_t topCount = 0;
	std::uint32_t candidates = 0;
	for (std::uint32_t index = 0; index < cache.count; ++index)
	{
		const std::uint32_t kind = cache.kinds[index];
		if ((allowedKinds & DLMOD_AIM_KIND_MASK(kind)) == 0)
			continue;
		const void *entity = cache.entities[index];
		DlModVector3V1 position{};
		if (!ReadPosition(*bindings, entity, position))
			continue;
		const float dx = position.x - center.x;
		const float dy = position.y - center.y;
		const float dz = position.z - center.z;
		const float distanceSquared = dx * dx + dy * dy + dz * dz;
		if (!std::isfinite(distanceSquared) ||
			distanceSquared > maximumDistanceSquared ||
			distanceSquared < minimumDistanceSquared)
		{
			continue;
		}
		float screenX = 0.0F;
		float screenY = 0.0F;
		if (!ProjectWithMatrix(
				camera.matrix, position, width, height, screenX, screenY))
		{
			continue;
		}
		const float screenOffsetX = screenX - centerX;
		const float screenOffsetY = screenY - centerY;
		const float screenDistance =
			screenOffsetX * screenOffsetX + screenOffsetY * screenOffsetY;
		if (fovRadius > 0.0F && screenDistance > fovRadiusSquared)
			continue;
		++candidates;

		AimCandidate candidate{};
		candidate.entity = entity;
		candidate.position = position;
		candidate.screenX = screenX;
		candidate.screenY = screenY;
		candidate.screenDistance = screenDistance;
		candidate.distance = std::sqrt(distanceSquared);
		candidate.kind = kind;
		InsertAimCandidate(top, topCount, refineLimit, candidate);
	}

	std::uint32_t refined = 0;
	float bestScore = 1.0e30F;
	AimCandidate best{};
	int bestBoneId = -1;
	for (std::size_t index = 0; index < topCount; ++index)
	{
		const AimCandidate &candidate = top[index];
		DlModVector3V1 point = candidate.position;
		std::int32_t boneId = -1;
		if (EnsureAimBoneId(*bindings, candidate.entity, head, boneId))
		{
			void *model = nullptr;
			DlModVector3V1 joint{};
			if (ReadModelForBones(candidate.entity, model) &&
				ReadBoneJoint(
					*bindings, model, static_cast<std::uint8_t>(boneId),
					joint))
			{
				point = joint;
			}
			else
			{
				boneId = -1;
			}
		}
		++refined;
		float screenX = 0.0F;
		float screenY = 0.0F;
		if (!ProjectWithMatrix(
				camera.matrix, point, width, height, screenX, screenY))
		{
			continue;
		}
		const float screenOffsetX = screenX - centerX;
		const float screenOffsetY = screenY - centerY;
		const float screenDistance =
			screenOffsetX * screenOffsetX + screenOffsetY * screenOffsetY;
		if (fovRadius > 0.0F && screenDistance > fovRadiusSquared)
			continue;
		const float score =
			screenDistance / screenScale * 0.7F +
			candidate.distance / radius * 0.3F;
		if (score >= bestScore)
			continue;
		bestScore = score;
		best = candidate;
		best.position = point;
		best.screenX = screenX;
		best.screenY = screenY;
		bestBoneId = boneId;
	}

	picked.scanned = scanned;
	picked.candidates = candidates;
	picked.refined = refined;
	picked.cached = reused ? 1U : 0U;
	if (best.entity != nullptr)
	{
		picked.validFields = DLMOD_AIM_PICK_HAS_TARGET;
		picked.kind = best.kind;
		picked.entity.instance = const_cast<void *>(best.entity);
		picked.position = best.position;
		picked.screenX = best.screenX;
		picked.screenY = best.screenY;
		if (bestBoneId >= 0)
		{
			picked.validFields |= DLMOD_AIM_PICK_HAS_BONE;
			picked.boneId = static_cast<uint32_t>(bestBoneId);
		}
	}
	*result = picked;
	return picked.validFields != 0 ? DLMOD_RESULT_SUCCESS
								   : DLMOD_RESULT_NOT_FOUND;
}

