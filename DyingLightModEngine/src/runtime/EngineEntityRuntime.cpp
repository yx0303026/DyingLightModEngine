#include "runtime/EngineEntityRuntime.h"

#include "runtime/EngineEntityBindings.h"
#include "tracking/EngineHunterTrackingRuntime.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{
	constexpr std::uint16_t kInvalidReplicationId = 0xFFFF;
	constexpr float kLocalSearchRadius = 500.0F;
	constexpr std::size_t kHealthCacheCapacity = 64;

	struct ControlObjectCollection
	{
		void** items;
		std::uint32_t count;
		std::uint32_t capacity;
		std::uint32_t flags;
	};

	struct HealthCacheEntry
	{
		const void* entity = nullptr;
		void* module = nullptr;
	};

	struct LocalEntityCache
	{
		const void* entity = nullptr;
		std::uint16_t replicationId = kInvalidReplicationId;
		ULONGLONG updatedAt = 0;
	};

	std::array<HealthCacheEntry, kHealthCacheCapacity> g_healthCache{};
	std::size_t g_healthCacheCursor = 0;
	LocalEntityCache g_localEntityCache{};
	SRWLOCK g_healthCacheLock = SRWLOCK_INIT;
	SRWLOCK g_localEntityCacheLock = SRWLOCK_INIT;

	template<typename Value>
	bool ReadField(const void* object, std::uintptr_t offset, Value& value) noexcept
	{
		value = {};
		if (object == nullptr)
			return false;
		__try
		{
			value = *reinterpret_cast<const Value*>(
				static_cast<const std::uint8_t*>(object) + offset);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			value = {};
			return false;
		}
	}

	bool IsFinite(const DlModVector3V1& value) noexcept
	{
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	DlModVector3V1 DeriveUp(
		const DlModVector3V1& forward,
		const DlModVector3V1& up) noexcept
	{
		const float upLength = up.x * up.x + up.y * up.y + up.z * up.z;
		if (IsFinite(up) && upLength > 1.0e-6F)
			return up;

		DlModVector3V1 axis{ 0.0F, 0.0F, 1.0F };
		if (std::fabs(forward.z) > 0.95F)
			axis = { 0.0F, 1.0F, 0.0F };
		const float dot = axis.x * forward.x + axis.y * forward.y +
			axis.z * forward.z;
		DlModVector3V1 result{
			axis.x - dot * forward.x,
			axis.y - dot * forward.y,
			axis.z - dot * forward.z
		};
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

	void* AsControlObject(const void* entity) noexcept
	{
		return entity == nullptr
			? nullptr
			: const_cast<std::uint8_t*>(
				static_cast<const std::uint8_t*>(entity)) +
				EngineEntityBindings::ControlObjectOffset;
	}

	std::uint8_t ReadModelId(const void* entity) noexcept
	{
		std::uint8_t modelId = 0;
		ReadField(
			AsControlObject(entity),
			EngineEntityBindings::HunterModelIdOffset,
			modelId);
		return modelId;
	}

	bool IsPlayer(const void* entity) noexcept
	{
		return EngineEntityBindings::IsDerivedFrom(entity, "PlayerDI");
	}

	bool IsHunter(const void* entity) noexcept
	{
		return entity != nullptr &&
			EngineEntityBindings::IsSupportedGame(EngineEntityBindings::GameModule()) &&
			ReadModelId(entity) == EngineEntityBindings::HunterModelId;
	}

	bool IsHuman(const void* entity) noexcept
	{
		return IsPlayer(entity) && !IsHunter(entity);
	}

	bool ReadReplicationId(
		const EngineEntityBindings& bindings,
		const void* entity,
		std::uint16_t& replicationId) noexcept
	{
		replicationId = kInvalidReplicationId;
		alignas(std::uint64_t) std::uint8_t returnBuffer[16]{};
		__try
		{
			void* idObject = bindings.getObjectId(
				const_cast<void*>(entity), returnBuffer);
			return ReadField(
				idObject, EngineEntityBindings::ReplicationIdOffset, replicationId);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			replicationId = kInvalidReplicationId;
			return false;
		}
	}

	void* FindLocalEntity(const EngineEntityBindings& bindings) noexcept
	{
		const HMODULE game = EngineEntityBindings::GameModule();
		std::uintptr_t gameInstance = 0;
		if (!ReadField(reinterpret_cast<void*>(game), EngineEntityBindings::GameGlobalRva, gameInstance) ||
			gameInstance == 0)
		{
			return nullptr;
		}

		void* level = nullptr;
		std::uint16_t localId = kInvalidReplicationId;
		__try
		{
			level = bindings.getActiveLevel(reinterpret_cast<void*>(gameInstance));
			localId = bindings.getLocalId(reinterpret_cast<void*>(gameInstance));
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
				return const_cast<void*>(cached.entity);
			}
		}

		DlModVector3V1 center{};
		void* camera = nullptr;
		__try
		{
			camera = bindings.getActiveCamera(level);
			if (camera != nullptr)
			{
				DlModVector3V1 cameraPosition{};
				if (bindings.getCameraPosition(camera, &cameraPosition) ==
					&cameraPosition && IsFinite(cameraPosition))
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
				nullptr, false, nullptr) || collection.items == nullptr)
			{
				std::free(collection.items);
				return nullptr;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			std::free(collection.items);
			return nullptr;
		}
		if (collection.count > collection.capacity || collection.count > 65536)
		{
			std::free(collection.items);
			return nullptr;
		}

		void* result = nullptr;
		for (std::uint32_t index = 0; index < collection.count; ++index)
		{
			void* control = nullptr;
			if (!ReadField(collection.items, index * sizeof(void*), control) ||
				control == nullptr)
			{
				continue;
			}
			auto* entity = static_cast<std::uint8_t*>(control) -
				EngineEntityBindings::ControlObjectOffset;
			std::uint16_t id = kInvalidReplicationId;
			if (ReadReplicationId(bindings, entity, id) && id == localId)
			{
				result = entity;
				break;
			}
		}
		std::free(collection.items);
		if (result != nullptr)
		{
			AcquireSRWLockExclusive(&g_localEntityCacheLock);
			g_localEntityCache = { result, localId, GetTickCount64() };
			ReleaseSRWLockExclusive(&g_localEntityCacheLock);
		}
		return result;
	}

	void* FindHealthModule(const void* entity) noexcept
	{
		AcquireSRWLockShared(&g_healthCacheLock);
		for (const HealthCacheEntry& entry : g_healthCache)
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
		const auto* bytes = static_cast<const std::uint8_t*>(entity);
		void* found = nullptr;
		for (std::uintptr_t offset = sizeof(void*);
			offset < EngineEntityBindings::ModuleScanSize;
			offset += sizeof(void*))
		{
			void* candidate = nullptr;
			if (!ReadField(bytes, offset, candidate) || candidate == nullptr)
				continue;
			void* vtable = nullptr;
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
			g_healthCache[g_healthCacheCursor] = { entity, found };
			g_healthCacheCursor = (g_healthCacheCursor + 1) % g_healthCache.size();
			ReleaseSRWLockExclusive(&g_healthCacheLock);
		}
		return found;
	}

	bool ReadHealth(const void* entity, float& body, float& bloodMask, std::uint64_t& damager) noexcept
	{
		body = 0.0F;
		bloodMask = 0.0F;
		damager = 0;
		void* module = FindHealthModule(entity);
		return module != nullptr &&
			ReadField(module, EngineEntityBindings::HealthBodyOffset, body) &&
			ReadField(module, EngineEntityBindings::HealthBloodMaskOffset, bloodMask) &&
			ReadField(module, EngineEntityBindings::HealthLastDamagerOffset, damager) &&
			std::isfinite(body) && std::isfinite(bloodMask);
	}

	bool ReadPosition(
		const EngineEntityBindings& bindings,
		const void* entity,
		DlModVector3V1& position) noexcept
	{
		position = {};
		DlModVector3V1 value{};
		__try
		{
			if (bindings.getPosition(
				AsControlObject(entity), &value) != &value || !IsFinite(value))
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

	bool ReadOrientation(
		const EngineEntityBindings& bindings,
		const void* entity,
		DlModVector3V1& forward,
		DlModVector3V1& up) noexcept
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
		const EngineEntityBindings& bindings,
		const void* entity,
		DlModEntitySnapshotV1* snapshot) noexcept
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
			result.level = bindings.getLevel(const_cast<void*>(entity));
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
		const std::size_t copySize = (std::min)(
			static_cast<std::size_t>(snapshot->structSize), sizeof(result));
		std::memcpy(snapshot, &result, copySize);
		return result.validFields == 0
			? DLMOD_RESULT_PARTIAL_SUCCESS
			: DLMOD_RESULT_SUCCESS;
	}

	bool IsValidHandle(const DlModEntityV1* entity) noexcept
	{
		return entity != nullptr && entity->instance != nullptr;
	}

}

const DlModEntityApiV1* EngineEntityRuntime::GetApi() noexcept
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
		&IsHunterTrackingInstalled
	};
#if defined(_M_X64)
	return EngineEntityBindings::Resolve() != nullptr ? &api : nullptr;
#else
	return nullptr;
#endif
}

void EngineEntityRuntime::Shutdown() noexcept
{
	EngineHunterTrackingRuntime::Shutdown();
}

DlModResult MODSDK_CALL EngineEntityRuntime::GetLocalEntity(DlModEntityV1* entity) noexcept
{
	if (entity == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	entity->instance = nullptr;
	const EngineEntityBindings* bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr)
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	entity->instance = FindLocalEntity(*bindings);
	return entity->instance != nullptr ? DLMOD_RESULT_SUCCESS : DLMOD_RESULT_NOT_FOUND;
}

DlModResult MODSDK_CALL EngineEntityRuntime::GetLocalPlayer(DlModPlayerV1* player) noexcept
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

DlModResult MODSDK_CALL EngineEntityRuntime::GetLocalHuman(DlModHumanV1* human) noexcept
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

DlModResult MODSDK_CALL EngineEntityRuntime::GetCurrentHunter(DlModHunterV1* hunter) noexcept
{
	if (hunter == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	hunter->instance = nullptr;
	void* control = EngineHunterTrackingRuntime::GetCurrentControlObject();
	if (control == nullptr ||
		!IsHunter(static_cast<std::uint8_t*>(control) -
			EngineEntityBindings::ControlObjectOffset))
	{
		return DLMOD_RESULT_NOT_FOUND;
	}
	hunter->instance = static_cast<std::uint8_t*>(control) -
		EngineEntityBindings::ControlObjectOffset;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadEntity(
	const DlModEntityV1* entity,
	DlModEntitySnapshotV1* snapshot) noexcept
{
	if (!IsValidHandle(entity))
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const EngineEntityBindings* bindings = EngineEntityBindings::Resolve();
	return bindings == nullptr
		? DLMOD_RESULT_UNSUPPORTED_FEATURE
		: ReadEntitySnapshot(*bindings, entity->instance, snapshot);
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadPlayer(
	const DlModPlayerV1* player,
	DlModPlayerSnapshotV1* snapshot) noexcept
{
	if (player == nullptr ||
		!IsValidHandle(reinterpret_cast<const DlModEntityV1*>(player)) ||
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
		reinterpret_cast<const DlModEntityV1*>(player),
		reinterpret_cast<DlModEntitySnapshotV1*>(snapshot));
	snapshot->playerFlags = 0;
	snapshot->structSize = size;
	return result;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadHuman(
	const DlModHumanV1* human,
	DlModHumanSnapshotV1* snapshot) noexcept
{
	if (human == nullptr || !IsValidHandle(reinterpret_cast<const DlModEntityV1*>(human)) ||
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
		reinterpret_cast<const DlModEntityV1*>(human),
		reinterpret_cast<DlModEntitySnapshotV1*>(snapshot));
	snapshot->playerFlags = 0;
	snapshot->humanFlags = 0;
	snapshot->structSize = size;
	return result;
}

DlModResult MODSDK_CALL EngineEntityRuntime::ReadHunter(
	const DlModHunterV1* hunter,
	DlModHunterSnapshotV1* snapshot) noexcept
{
	if (hunter == nullptr || !IsValidHandle(reinterpret_cast<const DlModEntityV1*>(hunter)) ||
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
		reinterpret_cast<const DlModEntityV1*>(hunter),
		reinterpret_cast<DlModEntitySnapshotV1*>(snapshot));
	snapshot->playerFlags = 0;
	EngineHunterTrackingRuntime::ReadMotion(
		AsControlObject(hunter->instance), snapshot->motion);
	snapshot->structSize = size;
	return result;
}

DlModResult MODSDK_CALL EngineEntityRuntime::SetPosition(
	const DlModEntityV1* entity,
	const DlModVector3V1* position) noexcept
{
	if (!IsValidHandle(entity) || position == nullptr || !IsFinite(*position))
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const EngineEntityBindings* bindings = EngineEntityBindings::Resolve();
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
	const DlModEntityV1* entity,
	const DlModVector3V1* forward,
	const DlModVector3V1* up) noexcept
{
	if (!IsValidHandle(entity) || forward == nullptr || up == nullptr ||
		!IsFinite(*forward) || !IsFinite(*up))
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	const EngineEntityBindings* bindings = EngineEntityBindings::Resolve();
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
	const DlModHunterV1* hunter,
	uint8_t boneId,
	DlModVector3V1* position) noexcept
{
	if (hunter == nullptr || !IsValidHandle(reinterpret_cast<const DlModEntityV1*>(hunter)) ||
		position == nullptr)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	*position = {};
	if (!IsHunter(hunter->instance))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	const EngineEntityBindings* bindings = EngineEntityBindings::Resolve();
	if (bindings == nullptr || bindings->getBonePosition == nullptr ||
		bindings->resolveBone == nullptr)
	{
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}
	void* control = AsControlObject(hunter->instance);
	void* model = nullptr;
	if (!ReadField(control, 0x08, model) || model == nullptr ||
		!EngineEntityBindings::IsDerivedFrom(model, "CModelObject"))
	{
		return DLMOD_RESULT_NOT_FOUND;
	}
	DlModVector3V1 value{};
	__try
	{
		if (bindings->resolveBone(control, boneId) < 0 ||
			bindings->getBonePosition(control, &value, boneId) != &value ||
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
	const DlModHunterV1* hunter,
	DlModHunterMotionV1* motion) noexcept
{
	if (hunter == nullptr || !IsValidHandle(reinterpret_cast<const DlModEntityV1*>(hunter)) ||
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
	return EngineHunterTrackingRuntime::Install()
		? DLMOD_RESULT_SUCCESS
		: DLMOD_RESULT_UNSUPPORTED_FEATURE;
}

uint8_t MODSDK_CALL EngineEntityRuntime::IsHunterTrackingInstalled() noexcept
{
	return EngineHunterTrackingRuntime::IsInstalled() ? 1U : 0U;
}
