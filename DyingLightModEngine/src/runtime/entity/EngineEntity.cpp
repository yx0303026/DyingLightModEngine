#include "runtime/entity/EngineEntity.h"

#include "runtime/EngineEntityRuntime.h"
#include "runtime/entity/damage/EngineDamageRuntime.h"

#include <cmath>
#include <cstddef>
#include <mutex>

namespace
{
	constexpr std::size_t kMaxDamageSubscriptions = 8;

	struct DamageSubscription
	{
		const void *entity = nullptr;
		ModEngineDamagePreHookFn pre = nullptr;
		ModEngineDamagePostHookFn post = nullptr;
	};

	std::mutex g_subscriptionMutex;
	DamageSubscription g_subscriptions[kMaxDamageSubscriptions]{};
	std::size_t g_subscriptionCount = 0;

	bool MatchesEvent(const DlModDamageEventV1 *event, const void *entity) noexcept
	{
		return event != nullptr && entity != nullptr && event->entity == entity;
	}

	/* 桥接回调：先按目标实体筛选，再把事件转发给订阅者。 */
	uint8_t MODSDK_CALL BridgeDamagePre(DlModDamageEventV1 *event) noexcept
	{
		DamageSubscription matches[kMaxDamageSubscriptions]{};
		std::size_t count = 0;
		{
			const std::lock_guard<std::mutex> guard(g_subscriptionMutex);
			for (std::size_t index = 0; index < g_subscriptionCount; ++index)
			{
				if (MatchesEvent(event, g_subscriptions[index].entity))
					matches[count++] = g_subscriptions[index];
			}
		}

		uint8_t allow = 1;
		for (std::size_t index = 0; index < count; ++index)
		{
			ModEngineDamagePreHookFn pre = matches[index].pre;
			if (pre == nullptr)
				continue;
			try
			{
				if (pre(event) == 0)
					allow = 0;
			}
			catch (...)
			{
			}
		}
		return allow;
	}

	void MODSDK_CALL BridgeDamagePost(const DlModDamageEventV1 *event) noexcept
	{
		DamageSubscription matches[kMaxDamageSubscriptions]{};
		std::size_t count = 0;
		{
			const std::lock_guard<std::mutex> guard(g_subscriptionMutex);
			for (std::size_t index = 0; index < g_subscriptionCount; ++index)
			{
				if (MatchesEvent(event, g_subscriptions[index].entity))
					matches[count++] = g_subscriptions[index];
			}
		}

		for (std::size_t index = 0; index < count; ++index)
		{
			ModEngineDamagePostHookFn post = matches[index].post;
			if (post == nullptr)
				continue;
			try
			{
				post(event);
			}
			catch (...)
			{
			}
		}
	}

	/* 安装伤害钩子；运行时重装说明上一轮已关闭，旧订阅指向的实体句柄不再可靠。 */
	DlModResult EnsureDamageHooks() noexcept
	{
		const bool wasInstalled = EngineDamageRuntime::IsInstalled();
		if (!EngineDamageRuntime::Install())
			return DLMOD_RESULT_UNSUPPORTED_FEATURE;
		if (!wasInstalled)
		{
			const std::lock_guard<std::mutex> guard(g_subscriptionMutex);
			g_subscriptionCount = 0;
			for (DamageSubscription &subscription : g_subscriptions)
				subscription = {};
		}
		return DLMOD_RESULT_SUCCESS;
	}

	/* 以下两个辅助函数要求调用方持有 g_subscriptionMutex。 */
	DlModResult EnsureBridgeRegistered() noexcept
	{
		if (EngineDamageRuntime::IsHookRegistered(&BridgeDamagePre, &BridgeDamagePost))
			return DLMOD_RESULT_SUCCESS;
		const DlModResult result =
			EngineDamageRuntime::RegisterHook(&BridgeDamagePre, &BridgeDamagePost);
		if (result == DLMOD_RESULT_INVALID_STATE &&
			EngineDamageRuntime::IsHookRegistered(&BridgeDamagePre, &BridgeDamagePost))
		{
			return DLMOD_RESULT_SUCCESS;
		}
		return result;
	}

	void ReleaseBridgeWhenIdle() noexcept
	{
		if (g_subscriptionCount != 0 ||
			!EngineDamageRuntime::IsHookRegistered(&BridgeDamagePre, &BridgeDamagePost))
		{
			return;
		}
		EngineDamageRuntime::UnregisterHook(&BridgeDamagePre, &BridgeDamagePost);
	}
}

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

DlModResult EngineEntity::InstallDamageTracking() noexcept
{
	return EnsureDamageHooks();
}

bool EngineEntity::IsDamageTrackingInstalled() noexcept
{
	return EngineDamageRuntime::IsInstalled();
}

DlModResult EngineEntity::RegisterDamageHook(
	ModEngineDamagePreHookFn pre,
	ModEngineDamagePostHookFn post) noexcept
{
	if (pre == nullptr && post == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const DlModResult ready = EnsureDamageHooks();
	if (ready != DLMOD_RESULT_SUCCESS)
		return ready;
	return EngineDamageRuntime::RegisterHook(pre, post);
}

DlModResult EngineEntity::UnregisterDamageHook(
	ModEngineDamagePreHookFn pre,
	ModEngineDamagePostHookFn post) noexcept
{
	return EngineDamageRuntime::UnregisterHook(pre, post);
}

DlModResult EngineEntity::SubscribeDamage(
	ModEngineDamagePreHookFn pre,
	ModEngineDamagePostHookFn post) const noexcept
{
	if (instance_ == nullptr || (pre == nullptr && post == nullptr))
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const DlModResult ready = EnsureDamageHooks();
	if (ready != DLMOD_RESULT_SUCCESS)
		return ready;

	const std::lock_guard<std::mutex> guard(g_subscriptionMutex);
	for (std::size_t index = 0; index < g_subscriptionCount; ++index)
	{
		const DamageSubscription &subscription = g_subscriptions[index];
		if (subscription.entity == instance_ && subscription.pre == pre &&
			subscription.post == post)
		{
			return DLMOD_RESULT_INVALID_STATE;
		}
	}
	if (g_subscriptionCount >= kMaxDamageSubscriptions)
		return DLMOD_RESULT_INVALID_STATE;

	const DlModResult registered = EnsureBridgeRegistered();
	if (registered != DLMOD_RESULT_SUCCESS)
		return registered;
	g_subscriptions[g_subscriptionCount] = DamageSubscription{instance_, pre, post};
	++g_subscriptionCount;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult EngineEntity::UnsubscribeDamage(
	ModEngineDamagePreHookFn pre,
	ModEngineDamagePostHookFn post) const noexcept
{
	if (instance_ == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;

	const std::lock_guard<std::mutex> guard(g_subscriptionMutex);
	for (std::size_t index = 0; index < g_subscriptionCount; ++index)
	{
		const DamageSubscription &subscription = g_subscriptions[index];
		if (subscription.entity != instance_ || subscription.pre != pre ||
			subscription.post != post)
		{
			continue;
		}
		g_subscriptions[index] = g_subscriptions[g_subscriptionCount - 1];
		g_subscriptions[g_subscriptionCount - 1] = {};
		--g_subscriptionCount;
		ReleaseBridgeWhenIdle();
		return DLMOD_RESULT_SUCCESS;
	}
	return DLMOD_RESULT_NOT_FOUND;
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
