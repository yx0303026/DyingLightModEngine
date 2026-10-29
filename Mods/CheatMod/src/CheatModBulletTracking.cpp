#include "CheatModBulletTracking.h"

#include <Windows.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace cheat_mod
{
	namespace
	{
		constexpr ImU32 kHumanColor = IM_COL32(255, 45, 45, 245);
		constexpr ImU32 kHunterColor = IM_COL32(255, 150, 40, 245);
		constexpr ImU32 kZombieColor = IM_COL32(80, 220, 120, 245);
		constexpr float kMarkerRadius = 7.0F;
		constexpr float kMarkerThickness = 2.0F;

		constexpr std::uint64_t kAcquireIntervalMs = 100;
		constexpr std::uint64_t kRefreshIntervalMs = 16;
		constexpr std::size_t kMaximumNearbyEntities = 128;
		constexpr float kMinimumSpeed = 0.001F;
		constexpr float kMaximumDeltaSeconds = 0.1F;
		constexpr float kMinimumTurnRadius = 0.0F;
		constexpr float kMaximumTurnRadius = 1000.0F;
		constexpr float kMinimumTurnRate = 0.0F;
		constexpr float kMaximumTurnRate = 100.0F;
		constexpr float kMinimumDistance = 10.0F;
		constexpr float kMaximumDistance = 500.0F;
		constexpr float kMaximumFovRadius = 2000.0F;
		/* 本地玩家始终贴近相机，必须排除，否则会抢占准星。 */
		constexpr float kMinimumTargetDistance = 2.0F;

		const char* const kHeadAliases[] = {
			"head", "Head", "head_joint", "Head_Joint", "Bip01 Head",
			"bip01_head", "Bip01_Head", "head_end", "Head_End", "neck",
			"Neck", "neck_joint", "Bip01 Neck", "bip01_neck", "Bip01_Neck",
			"neck_end"
		};
		const char* const kBodyAliases[] = {
			"spine", "Spine", "spine1", "Spine1", "spine2", "Spine2",
			"Bip01 Spine", "Bip01 Spine1", "Bip01 Spine2", "bip01_spine",
			"bip01_spine1", "bip01_spine2", "chest", "Chest", "upperchest",
			"torso"
		};

		bool IsFinite(const DlModVector3V1& value) noexcept
		{
			return std::isfinite(value.x) && std::isfinite(value.y) &&
				std::isfinite(value.z);
		}

		float Length(const DlModVector3V1& value) noexcept
		{
			return std::sqrt(value.x * value.x + value.y * value.y +
				value.z * value.z);
		}

		float ClampFloat(
			float value,
			float minimum,
			float maximum,
			float fallback) noexcept
		{
			if (!std::isfinite(value))
				return fallback;
			return (std::clamp)(value, minimum, maximum);
		}

		ImU32 MarkerColor(std::uint32_t kind) noexcept
		{
			switch (kind)
			{
			case DLMOD_ENTITY_KIND_HUNTER:
				return kHunterColor;
			case DLMOD_ENTITY_KIND_ZOMBIE:
				return kZombieColor;
			default:
				return kHumanColor;
			}
		}

		std::uint32_t ProjectileBit(std::uint32_t kind) noexcept
		{
			switch (kind)
			{
			case DLMOD_PROJECTILE_KIND_ARROW:
				return DLMOD_PROJECTILE_FILTER_ARROW;
			case DLMOD_PROJECTILE_KIND_BOLT:
				return DLMOD_PROJECTILE_FILTER_BOLT;
			case DLMOD_PROJECTILE_KIND_BULLET:
				return DLMOD_PROJECTILE_FILTER_BULLET;
			default:
				return 0;
			}
		}
	}

	void CheatModBulletTracking::SetProjectileApi(
		const DlModProjectileManagerApiV1* api) noexcept
	{
		projectile_ = api;
	}

	void CheatModBulletTracking::SetCameraApi(const DlModCameraApiV1* api) noexcept
	{
		camera_ = api;
	}

	void CheatModBulletTracking::SetEntityApi(const DlModEntityApiV1* api) noexcept
	{
		entity_ = api;
	}

	bool CheatModBulletTracking::IsAvailable() const noexcept
	{
		return projectile_ != nullptr && camera_ != nullptr && entity_ != nullptr &&
			projectile_->install != nullptr &&
			projectile_->setUpdateCallback != nullptr &&
			projectile_->setFrameCallback != nullptr &&
			projectile_->readPosition != nullptr &&
			projectile_->readVelocity != nullptr &&
			projectile_->writeVelocity != nullptr &&
			camera_->readPosition != nullptr &&
			camera_->project != nullptr &&
			entity_->enumerateNearby != nullptr &&
			entity_->readBoneByName != nullptr;
	}

	void CheatModBulletTracking::SetEnabled(bool enabled) noexcept
	{
		if (enabled && !IsAvailable())
		{
			enabled_.store(false, std::memory_order_release);
			return;
		}
		if (enabled && !installed_.load(std::memory_order_relaxed))
		{
			if (projectile_->install() != DLMOD_RESULT_SUCCESS)
			{
				enabled_.store(false, std::memory_order_release);
				return;
			}
			projectile_->setUpdateCallback(
				this, &CheatModBulletTracking::OnProjectileUpdate);
			projectile_->setFrameCallback(
				this, &CheatModBulletTracking::OnFrame);
			installed_.store(true, std::memory_order_release);
		}
		enabled_.store(enabled, std::memory_order_release);
		if (!enabled)
		{
			ClearTarget();
		}
	}

	bool CheatModBulletTracking::IsEnabled() const noexcept
	{
		return enabled_.load(std::memory_order_acquire);
	}

	void CheatModBulletTracking::Shutdown() noexcept
	{
		enabled_.store(false, std::memory_order_release);
		if (projectile_ != nullptr &&
			installed_.exchange(false, std::memory_order_acq_rel))
		{
			if (projectile_->setUpdateCallback != nullptr)
				projectile_->setUpdateCallback(nullptr, nullptr);
			if (projectile_->setFrameCallback != nullptr)
				projectile_->setFrameCallback(nullptr, nullptr);
		}
		ClearTarget();
	}

	void CheatModBulletTracking::SetShowTarget(bool show) noexcept
	{
		showTarget_.store(show, std::memory_order_relaxed);
	}

	bool CheatModBulletTracking::IsShowTarget() const noexcept
	{
		return showTarget_.load(std::memory_order_relaxed);
	}

	void CheatModBulletTracking::SetLockMode(int index) noexcept
	{
		lockMode_.store(index == 1 ? 1U : 0U, std::memory_order_relaxed);
		ClearTarget();
	}

	int CheatModBulletTracking::GetLockMode() const noexcept
	{
		return static_cast<int>(lockMode_.load(std::memory_order_relaxed));
	}

	void CheatModBulletTracking::SetTargetKind(int index, bool enabled) noexcept
	{
		switch (index)
		{
		case 0:
			targetHuman_.store(enabled, std::memory_order_relaxed);
			break;
		case 1:
			targetHunter_.store(enabled, std::memory_order_relaxed);
			break;
		case 2:
			targetZombie_.store(enabled, std::memory_order_relaxed);
			break;
		default:
			return;
		}
		ClearTarget();
	}

	bool CheatModBulletTracking::IsTargetKindEnabled(int index) const noexcept
	{
		switch (index)
		{
		case 0:
			return targetHuman_.load(std::memory_order_relaxed);
		case 1:
			return targetHunter_.load(std::memory_order_relaxed);
		case 2:
			return targetZombie_.load(std::memory_order_relaxed);
		default:
			return false;
		}
	}

	void CheatModBulletTracking::SetProjectileKind(int index, bool enabled) noexcept
	{
		std::uint32_t bit = 0;
		switch (index)
		{
		case 0:
			bit = DLMOD_PROJECTILE_FILTER_ARROW;
			break;
		case 1:
			bit = DLMOD_PROJECTILE_FILTER_BOLT;
			break;
		case 2:
			bit = DLMOD_PROJECTILE_FILTER_BULLET;
			break;
		default:
			return;
		}
		std::uint32_t kinds = kinds_.load(std::memory_order_relaxed);
		if (enabled)
			kinds |= bit;
		else
			kinds &= ~bit;
		kinds_.store(kinds, std::memory_order_relaxed);
	}

	bool CheatModBulletTracking::IsProjectileKindEnabled(int index) const noexcept
	{
		const std::uint32_t kinds = kinds_.load(std::memory_order_relaxed);
		switch (index)
		{
		case 0:
			return (kinds & DLMOD_PROJECTILE_FILTER_ARROW) != 0;
		case 1:
			return (kinds & DLMOD_PROJECTILE_FILTER_BOLT) != 0;
		case 2:
			return (kinds & DLMOD_PROJECTILE_FILTER_BULLET) != 0;
		default:
			return false;
		}
	}

	void CheatModBulletTracking::SetPrecision(float value) noexcept
	{
		precision_.store(
			ClampFloat(value, 0.0F, 1.0F, precision_.load(std::memory_order_relaxed)),
			std::memory_order_relaxed);
	}

	float CheatModBulletTracking::GetPrecision() const noexcept
	{
		return precision_.load(std::memory_order_relaxed);
	}

	void CheatModBulletTracking::SetTurnRate(float value) noexcept
	{
		turnRate_.store(
			ClampFloat(
				value, kMinimumTurnRate, kMaximumTurnRate,
				turnRate_.load(std::memory_order_relaxed)),
			std::memory_order_relaxed);
	}

	float CheatModBulletTracking::GetTurnRate() const noexcept
	{
		return turnRate_.load(std::memory_order_relaxed);
	}

	void CheatModBulletTracking::SetTurnRadius(float value) noexcept
	{
		turnRadius_.store(
			ClampFloat(
				value, kMinimumTurnRadius, kMaximumTurnRadius,
				turnRadius_.load(std::memory_order_relaxed)),
			std::memory_order_relaxed);
	}

	float CheatModBulletTracking::GetTurnRadius() const noexcept
	{
		return turnRadius_.load(std::memory_order_relaxed);
	}

	void CheatModBulletTracking::SetMaximumDistance(float value) noexcept
	{
		maximumDistance_.store(
			ClampFloat(
				value, kMinimumDistance, kMaximumDistance,
				maximumDistance_.load(std::memory_order_relaxed)),
			std::memory_order_relaxed);
		ClearTarget();
	}

	float CheatModBulletTracking::GetMaximumDistance() const noexcept
	{
		return maximumDistance_.load(std::memory_order_relaxed);
	}

	void CheatModBulletTracking::SetFovRadius(float value) noexcept
	{
		fovRadius_.store(
			ClampFloat(
				value, 0.0F, kMaximumFovRadius,
				fovRadius_.load(std::memory_order_relaxed)),
			std::memory_order_relaxed);
		ClearTarget();
	}

	float CheatModBulletTracking::GetFovRadius() const noexcept
	{
		return fovRadius_.load(std::memory_order_relaxed);
	}

	void CheatModBulletTracking::Refresh() noexcept
	{
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		if (display.x > 0.0F && display.y > 0.0F)
		{
			viewportWidth_.store(display.x, std::memory_order_relaxed);
			viewportHeight_.store(display.y, std::memory_order_relaxed);
		}

		target_ = {};
		if (targetValid_.load(std::memory_order_acquire))
		{
			target_.valid = true;
			target_.kind = targetKind_.load(std::memory_order_relaxed);
			target_.position = {
				targetX_.load(std::memory_order_relaxed),
				targetY_.load(std::memory_order_relaxed),
				targetZ_.load(std::memory_order_relaxed)
			};
			target_.hasScreen = targetHasScreen_.load(std::memory_order_relaxed);
			target_.screenX = targetScreenX_.load(std::memory_order_relaxed);
			target_.screenY = targetScreenY_.load(std::memory_order_relaxed);
		}

		stats_.updates = updateCount_.load(std::memory_order_relaxed);
		stats_.candidates = candidateCount_.load(std::memory_order_relaxed);
		stats_.adjusted = adjustedCount_.load(std::memory_order_relaxed);
		stats_.targetMatches = targetMatchCount_.load(std::memory_order_relaxed);
		stats_.directionFailures = directionFailureCount_.load(
			std::memory_order_relaxed);
		stats_.acquisitions = acquisitionCount_.load(std::memory_order_relaxed);
		stats_.targetsFound = targetsFound_.load(std::memory_order_relaxed);
	}

	void CheatModBulletTracking::DrawOverlay() const noexcept
	{
		if (!IsEnabled() || !IsShowTarget() || !target_.valid ||
			!target_.hasScreen)
		{
			return;
		}
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		if (target_.screenX < 0.0F || target_.screenY < 0.0F ||
			target_.screenX > display.x || target_.screenY > display.y)
		{
			return;
		}
		const ImU32 color = MarkerColor(target_.kind);
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		draw->AddCircle(
			ImVec2(target_.screenX, target_.screenY),
			kMarkerRadius,
			color,
			24,
			kMarkerThickness);
		draw->AddCircleFilled(
			ImVec2(target_.screenX, target_.screenY), 1.5F, color, 8);
	}

	const BulletTrackingTarget& CheatModBulletTracking::GetTarget() const noexcept
	{
		return target_;
	}

	const BulletTrackingStats& CheatModBulletTracking::GetStats() const noexcept
	{
		return stats_;
	}

	const char* CheatModBulletTracking::GetStatusText() const noexcept
	{
		if (!IsAvailable())
			return "Engine projectile API unavailable";
		if (!installed_.load(std::memory_order_relaxed))
			return IsEnabled() ? "Hook installation failed" : "Idle";
		return IsEnabled() ? "Tracking" : "Idle";
	}

	void MODSDK_CALL CheatModBulletTracking::OnProjectileUpdate(
		void* context,
		const DlModProjectileUpdateV1* update) noexcept
	{
		if (context == nullptr || update == nullptr)
			return;
		auto* self = static_cast<CheatModBulletTracking*>(context);
		self->updateCount_.fetch_add(1, std::memory_order_relaxed);
		if (!self->enabled_.load(std::memory_order_relaxed))
			return;
		/* 帧 Hook 缺失时由弹丸更新驱动目标维护。 */
		if (!self->frameCallbackSeen_.load(std::memory_order_relaxed))
			self->Tick();
		self->SteerProjectile(*update);
	}

	void MODSDK_CALL CheatModBulletTracking::OnFrame(void* context) noexcept
	{
		if (context == nullptr)
			return;
		auto* self = static_cast<CheatModBulletTracking*>(context);
		self->frameCallbackSeen_.store(true, std::memory_order_relaxed);
		self->Tick();
	}

	void CheatModBulletTracking::Tick() noexcept
	{
		if (!enabled_.load(std::memory_order_relaxed))
			return;
		const std::uint64_t now = GetTickCount64();
		if (now - lastAcquireTick_ >= kAcquireIntervalMs)
		{
			lastAcquireTick_ = now;
			lastRefreshTick_ = now;
			AcquireTarget();
			return;
		}
		if (now - lastRefreshTick_ >= kRefreshIntervalMs)
		{
			lastRefreshTick_ = now;
			RefreshTargetPosition();
		}
	}

	void CheatModBulletTracking::AcquireTarget() noexcept
	{
		acquisitionCount_.fetch_add(1, std::memory_order_relaxed);
		const float width = viewportWidth_.load(std::memory_order_relaxed);
		const float height = viewportHeight_.load(std::memory_order_relaxed);
		const float maximumDistance = maximumDistance_.load(
			std::memory_order_relaxed);
		if (!(width > 0.0F) || !(height > 0.0F) || !(maximumDistance > 0.0F))
		{
			ClearTarget();
			return;
		}

		DlModVector3V1 cameraPosition{};
		if (camera_->readPosition(&cameraPosition) != DLMOD_RESULT_SUCCESS ||
			!IsFinite(cameraPosition))
		{
			ClearTarget();
			return;
		}

		DlModNearbyEntityV1 nearby[kMaximumNearbyEntities]{};
		std::uint32_t count = 0;
		const DlModResult query = entity_->enumerateNearby(
			&cameraPosition,
			maximumDistance,
			nearby,
			static_cast<std::uint32_t>(kMaximumNearbyEntities),
			&count);
		if (query != DLMOD_RESULT_SUCCESS &&
			query != DLMOD_RESULT_PARTIAL_SUCCESS)
		{
			ClearTarget();
			return;
		}
		if (count > kMaximumNearbyEntities)
			count = static_cast<std::uint32_t>(kMaximumNearbyEntities);

		const float fovRadius = fovRadius_.load(std::memory_order_relaxed);
		const float centerX = width * 0.5F;
		const float centerY = height * 0.5F;
		const float screenScale = width * width + height * height;
		const float maximumDistanceSquared = maximumDistance * maximumDistance;
		const float minimumDistanceSquared =
			kMinimumTargetDistance * kMinimumTargetDistance;
		void* localInstance = nullptr;
		if (entity_->getLocalEntity != nullptr)
		{
			DlModEntityV1 local{};
			if (entity_->getLocalEntity(&local) == DLMOD_RESULT_SUCCESS)
				localInstance = local.instance;
		}
		float bestScore = 1.0e30F;
		std::uint32_t found = 0;
		std::uint32_t bestKind = 0;
		DlModEntityV1 bestEntity{};
		DlModVector3V1 bestPoint{};
		float bestScreenX = 0.0F;
		float bestScreenY = 0.0F;

		for (std::uint32_t index = 0; index < count; ++index)
		{
			const DlModNearbyEntityV1& item = nearby[index];
			if ((item.validFields & DLMOD_ENTITY_HAS_POSITION) == 0 ||
				!IsKindAllowed(item.kind))
			{
				continue;
			}
			if (localInstance != nullptr && item.entity.instance == localInstance)
				continue;
			const float offsetX = item.position.x - cameraPosition.x;
			const float offsetY = item.position.y - cameraPosition.y;
			const float offsetZ = item.position.z - cameraPosition.z;
			const float distanceSquared = offsetX * offsetX +
				offsetY * offsetY + offsetZ * offsetZ;
			if (!std::isfinite(distanceSquared) ||
				distanceSquared > maximumDistanceSquared ||
				distanceSquared < minimumDistanceSquared)
			{
				continue;
			}

			DlModVector3V1 point{};
			if (!ReadTargetPoint(item.entity, point))
				continue;
			float screenX = 0.0F;
			float screenY = 0.0F;
			if (camera_->project(
				&point, width, height, &screenX, &screenY) !=
				DLMOD_RESULT_SUCCESS)
			{
				continue;
			}
			const float screenOffsetX = screenX - centerX;
			const float screenOffsetY = screenY - centerY;
			const float screenDistance = screenOffsetX * screenOffsetX +
				screenOffsetY * screenOffsetY;
			if (fovRadius > 0.0F && screenDistance > fovRadius * fovRadius)
				continue;

			++found;
			const float score = screenDistance / screenScale * 0.7F +
				std::sqrt(distanceSquared) / maximumDistance * 0.3F;
			if (score >= bestScore)
				continue;
			bestScore = score;
			bestKind = item.kind;
			bestEntity = item.entity;
			bestPoint = point;
			bestScreenX = screenX;
			bestScreenY = screenY;
		}
		targetsFound_.store(found, std::memory_order_relaxed);

		if (bestEntity.instance == nullptr)
		{
			ClearTarget();
			return;
		}
		targetEntity_.store(bestEntity.instance, std::memory_order_relaxed);
		targetKind_.store(bestKind, std::memory_order_relaxed);
		targetX_.store(bestPoint.x, std::memory_order_relaxed);
		targetY_.store(bestPoint.y, std::memory_order_relaxed);
		targetZ_.store(bestPoint.z, std::memory_order_relaxed);
		targetScreenX_.store(bestScreenX, std::memory_order_relaxed);
		targetScreenY_.store(bestScreenY, std::memory_order_relaxed);
		targetHasScreen_.store(true, std::memory_order_relaxed);
		targetValid_.store(true, std::memory_order_release);
	}

	void CheatModBulletTracking::RefreshTargetPosition() noexcept
	{
		if (!targetValid_.load(std::memory_order_relaxed))
			return;
		void* instance = targetEntity_.load(std::memory_order_relaxed);
		if (instance == nullptr)
			return;
		const DlModEntityV1 target{ instance };
		DlModVector3V1 point{};
		if (!ReadTargetPoint(target, point))
			return;
		targetX_.store(point.x, std::memory_order_relaxed);
		targetY_.store(point.y, std::memory_order_relaxed);
		targetZ_.store(point.z, std::memory_order_relaxed);

		const float width = viewportWidth_.load(std::memory_order_relaxed);
		const float height = viewportHeight_.load(std::memory_order_relaxed);
		float screenX = 0.0F;
		float screenY = 0.0F;
		if (camera_->project(
			&point, width, height, &screenX, &screenY) ==
			DLMOD_RESULT_SUCCESS)
		{
			targetScreenX_.store(screenX, std::memory_order_relaxed);
			targetScreenY_.store(screenY, std::memory_order_relaxed);
			targetHasScreen_.store(true, std::memory_order_relaxed);
		}
		else
		{
			targetHasScreen_.store(false, std::memory_order_relaxed);
		}
	}

	void CheatModBulletTracking::SteerProjectile(
		const DlModProjectileUpdateV1& update) noexcept
	{
		if (update.record == nullptr)
			return;
		const std::uint32_t bit = ProjectileBit(update.kind);
		if (bit == 0 ||
			(kinds_.load(std::memory_order_relaxed) & bit) == 0)
		{
			return;
		}
		candidateCount_.fetch_add(1, std::memory_order_relaxed);
		if (!targetValid_.load(std::memory_order_relaxed))
			return;

		DlModVector3V1 position{};
		DlModVector3V1 velocity{};
		if (projectile_->readPosition(update.record, &position) !=
			DLMOD_RESULT_SUCCESS ||
			projectile_->readVelocity(update.record, &velocity) !=
			DLMOD_RESULT_SUCCESS)
		{
			return;
		}
		const float speed = Length(velocity);
		if (!(speed > kMinimumSpeed))
			return;
		const DlModVector3V1 target{
			targetX_.load(std::memory_order_relaxed),
			targetY_.load(std::memory_order_relaxed),
			targetZ_.load(std::memory_order_relaxed)
		};
		if (!IsFinite(target))
			return;
		const DlModVector3V1 delta{
			target.x - position.x,
			target.y - position.y,
			target.z - position.z
		};
		const float distance = Length(delta);
		if (!(distance > kMinimumSpeed))
			return;
		targetMatchCount_.fetch_add(1, std::memory_order_relaxed);

		/* 同一帧内保持参数一致，避免 UI 修改造成混读。 */
		const float turnRate = turnRate_.load(std::memory_order_relaxed);
		const float turnRadius = turnRadius_.load(std::memory_order_relaxed);
		const float precision = precision_.load(std::memory_order_relaxed);

		float step = update.deltaSeconds;
		if (!std::isfinite(step) || step <= 0.0F)
			step = 1.0F / 60.0F;
		step = (std::min)(step, kMaximumDeltaSeconds);

		const float radiusBlend = turnRadius <= 0.0F
			? 1.0F
			: (std::min)(1.0F, speed * step / turnRadius);
		const float response = turnRadius <= 0.0F
			? 1.0F
			: 1.0F - std::exp(-turnRate * step);
		const float blend = response * radiusBlend * precision;
		if (!(blend > 0.0F))
			return;

		const DlModVector3V1 current{
			velocity.x / speed,
			velocity.y / speed,
			velocity.z / speed
		};
		const DlModVector3V1 desired{
			delta.x / distance,
			delta.y / distance,
			delta.z / distance
		};
		const DlModVector3V1 blended{
			current.x + (desired.x - current.x) * blend,
			current.y + (desired.y - current.y) * blend,
			current.z + (desired.z - current.z) * blend
		};
		const float length = Length(blended);
		if (!(length > kMinimumSpeed))
			return;
		const float scale = speed / length;
		const DlModVector3V1 result{
			blended.x * scale,
			blended.y * scale,
			blended.z * scale
		};
		if (projectile_->writeVelocity(update.record, &result) ==
			DLMOD_RESULT_SUCCESS)
		{
			adjustedCount_.fetch_add(1, std::memory_order_relaxed);
		}
		else
		{
			directionFailureCount_.fetch_add(1, std::memory_order_relaxed);
		}
	}

	bool CheatModBulletTracking::ReadTargetPoint(
		const DlModEntityV1& entity,
		DlModVector3V1& point) noexcept
	{
		const bool head = lockMode_.load(std::memory_order_relaxed) == 1;
		const char* const* aliases = head ? kHeadAliases : kBodyAliases;
		const std::size_t count = head
			? std::size(kHeadAliases)
			: std::size(kBodyAliases);
		std::uint8_t& hint = head ? headAliasHint_ : bodyAliasHint_;
		for (std::size_t attempt = 0; attempt < count; ++attempt)
		{
			/* 优先复用上次命中的别名，减少每轮骨骼解析开销。 */
			const std::size_t index = hint < count
				? (static_cast<std::size_t>(hint) + attempt) % count
				: attempt;
			if (entity_->readBoneByName(&entity, aliases[index], &point) ==
				DLMOD_RESULT_SUCCESS)
			{
				hint = static_cast<std::uint8_t>(index);
				return true;
			}
		}
		return false;
	}

	bool CheatModBulletTracking::IsKindAllowed(std::uint32_t kind) const noexcept
	{
		switch (kind)
		{
		case DLMOD_ENTITY_KIND_HUMAN:
		case DLMOD_ENTITY_KIND_PLAYER:
			return targetHuman_.load(std::memory_order_relaxed);
		case DLMOD_ENTITY_KIND_HUNTER:
			return targetHunter_.load(std::memory_order_relaxed);
		case DLMOD_ENTITY_KIND_ZOMBIE:
			return targetZombie_.load(std::memory_order_relaxed);
		default:
			return false;
		}
	}

	void CheatModBulletTracking::ClearTarget() noexcept
	{
		targetValid_.store(false, std::memory_order_release);
		targetHasScreen_.store(false, std::memory_order_relaxed);
		targetKind_.store(0, std::memory_order_relaxed);
		targetEntity_.store(nullptr, std::memory_order_relaxed);
	}
}
