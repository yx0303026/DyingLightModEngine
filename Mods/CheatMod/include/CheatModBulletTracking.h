#pragma once

#include "sdk/EngineCameraAbi.h"
#include "sdk/EngineEntityAbi.h"
#include "sdk/EngineProjectileAbi.h"

#include <atomic>
#include <cstdint>

namespace cheat_mod
{
	struct BulletTrackingTarget final
	{
		std::uint32_t kind = 0;
		DlModVector3V1 position{};
		float screenX = 0.0F;
		float screenY = 0.0F;
		bool valid = false;
		bool hasScreen = false;
	};

	struct BulletTrackingStats final
	{
		std::uint32_t updates = 0;
		std::uint32_t candidates = 0;
		std::uint32_t adjusted = 0;
		std::uint32_t targetMatches = 0;
		std::uint32_t directionFailures = 0;
		std::uint32_t acquisitions = 0;
		std::uint32_t targetsFound = 0;
		/* 目标获取各阶段计数，用于定位“Targets 0”的失败环节。 */
		std::uint32_t nearby = 0;
		std::uint32_t kindMatches = 0;
		std::uint32_t boneMatches = 0;
		std::uint32_t projected = 0;
		/* 枚举结果的原始种类分布，用于确认查询是否只命中静态物体。 */
		std::uint32_t humans = 0;
		std::uint32_t hunters = 0;
		std::uint32_t zombies = 0;
		std::uint32_t entities = 0;
		/* 枚举结果被容量截断时为 1。 */
		std::uint32_t truncated = 0;
		/* 引擎侧准星拾取：实际精算的候选数，以及是否复用了角色缓存。 */
		std::uint32_t aimRefined = 0;
		std::uint32_t aimCached = 0;
		/* 1 表示当前走引擎单次拾取路径，0 表示回退到逐实体枚举。 */
		std::uint32_t aimPick = 0;
	};

	// 子弹追踪：在引擎弹丸回调内锁定目标并修正弹道，在渲染帧绘制准星标记。
	class CheatModBulletTracking final
	{
	public:
		CheatModBulletTracking() = default;
		~CheatModBulletTracking() = default;
		CheatModBulletTracking(const CheatModBulletTracking&) = delete;
		CheatModBulletTracking& operator=(const CheatModBulletTracking&) = delete;

		void SetProjectileApi(const DlModProjectileManagerApiV1* api) noexcept;
		void SetCameraApi(const DlModCameraApiV1* api) noexcept;
		void SetEntityApi(const DlModEntityApiV1* api) noexcept;
		bool IsAvailable() const noexcept;
		/* 引擎提供单次准星拾取时走快路径，否则回退到逐实体枚举。 */
		bool HasAimPick() const noexcept;

		void SetEnabled(bool enabled) noexcept;
		bool IsEnabled() const noexcept;
		/* 卸载时清除引擎回调，避免悬挂上下文。 */
		void Shutdown() noexcept;
		void SetShowTarget(bool show) noexcept;
		bool IsShowTarget() const noexcept;

		/* 0 = body, 1 = head */
		void SetLockMode(int index) noexcept;
		int GetLockMode() const noexcept;
		/* 0 = human, 1 = hunter, 2 = zombie */
		void SetTargetKind(int index, bool enabled) noexcept;
		bool IsTargetKindEnabled(int index) const noexcept;
		/* 0 = arrow, 1 = bolt, 2 = bullet */
		void SetProjectileKind(int index, bool enabled) noexcept;
		bool IsProjectileKindEnabled(int index) const noexcept;

		void SetPrecision(float value) noexcept;
		float GetPrecision() const noexcept;
		void SetTurnRate(float value) noexcept;
		float GetTurnRate() const noexcept;
		void SetTurnRadius(float value) noexcept;
		float GetTurnRadius() const noexcept;
		void SetMaximumDistance(float value) noexcept;
		float GetMaximumDistance() const noexcept;
		void SetFovRadius(float value) noexcept;
		float GetFovRadius() const noexcept;

		/* 每帧调用：上报视口并刷新目标与统计快照。 */
		void Refresh() noexcept;
		void DrawOverlay() const noexcept;

		const BulletTrackingTarget& GetTarget() const noexcept;
		const BulletTrackingStats& GetStats() const noexcept;
		const char* GetStatusText() const noexcept;

	private:
		static void MODSDK_CALL OnProjectileUpdate(
			void* context,
			const DlModProjectileUpdateV1* update) noexcept;
		static void MODSDK_CALL OnFrame(void* context) noexcept;

		void Tick() noexcept;
		void AcquireTarget() noexcept;
		void AcquireTargetLegacy() noexcept;
		void AcquireTargetFast() noexcept;
		void RefreshTargetPosition() noexcept;
		void SteerProjectile(const DlModProjectileUpdateV1& update) noexcept;
		bool ReadTargetPoint(const DlModEntityV1& entity, DlModVector3V1& point) noexcept;
		bool IsKindAllowed(std::uint32_t kind) const noexcept;
		void ClearTarget() noexcept;

		const DlModProjectileManagerApiV1* projectile_ = nullptr;
		const DlModCameraApiV1* camera_ = nullptr;
		const DlModEntityApiV1* entity_ = nullptr;

		std::atomic<bool> enabled_{ false };
		std::atomic<bool> installed_{ false };
		std::atomic<bool> showTarget_{ true };
		/* 帧 Hook 可用后不再由弹丸回调驱动目标维护。 */
		std::atomic<bool> frameCallbackSeen_{ false };

		/* UI 线程写入、游戏线程读取的配置快照。 */
		std::atomic<std::uint32_t> lockMode_{ 0 };
		std::atomic<std::uint32_t> kinds_{ DLMOD_PROJECTILE_FILTER_DEFAULT };
		std::atomic<bool> targetHuman_{ true };
		std::atomic<bool> targetHunter_{ true };
		std::atomic<bool> targetZombie_{ false };
		std::atomic<float> precision_{ 1.0F };
		std::atomic<float> turnRate_{ 7.0F };
		std::atomic<float> turnRadius_{ 0.0F };
		std::atomic<float> maximumDistance_{ 120.0F };
		std::atomic<float> fovRadius_{ 0.0F };

		std::atomic<float> viewportWidth_{ 0.0F };
		std::atomic<float> viewportHeight_{ 0.0F };

		std::atomic<bool> targetValid_{ false };
		std::atomic<std::uint32_t> targetKind_{ 0 };
		std::atomic<float> targetX_{ 0.0F };
		std::atomic<float> targetY_{ 0.0F };
		std::atomic<float> targetZ_{ 0.0F };
		std::atomic<float> targetScreenX_{ 0.0F };
		std::atomic<float> targetScreenY_{ 0.0F };
		std::atomic<bool> targetHasScreen_{ false };
		/* 引擎拾取返回的骨骼 ID：逐帧只用 ID 重复读取，不再做字符串查找。 */
		std::atomic<std::uint32_t> targetBoneId_{ 0 };
		std::atomic<bool> targetHasBone_{ false };

		std::atomic<std::uint32_t> updateCount_{ 0 };
		std::atomic<std::uint32_t> candidateCount_{ 0 };
		std::atomic<std::uint32_t> adjustedCount_{ 0 };
		std::atomic<std::uint32_t> targetMatchCount_{ 0 };
		std::atomic<std::uint32_t> directionFailureCount_{ 0 };
		std::atomic<std::uint32_t> acquisitionCount_{ 0 };
		std::atomic<std::uint32_t> targetsFound_{ 0 };
		std::atomic<std::uint32_t> nearbyCount_{ 0 };
		std::atomic<std::uint32_t> kindCount_{ 0 };
		std::atomic<std::uint32_t> boneCount_{ 0 };
		std::atomic<std::uint32_t> projectedCount_{ 0 };
		std::atomic<std::uint32_t> humanCount_{ 0 };
		std::atomic<std::uint32_t> hunterCount_{ 0 };
		std::atomic<std::uint32_t> zombieCount_{ 0 };
		std::atomic<std::uint32_t> entityCount_{ 0 };
		std::atomic<std::uint32_t> truncated_{ 0 };
		std::atomic<std::uint32_t> aimRefinedCount_{ 0 };
		std::atomic<std::uint32_t> aimCachedCount_{ 0 };
		std::atomic<std::uint32_t> aimPickEnabled_{ 0 };

		/* 仅游戏线程访问。 */
		std::uint64_t lastAcquireTick_ = 0;
		std::uint64_t lastRefreshTick_ = 0;
		std::uint8_t headAliasHint_ = 0xFF;
		std::uint8_t bodyAliasHint_ = 0xFF;
		/* 锁定目标实例，UI 线程清理与游戏线程刷新共享。 */
		std::atomic<void*> targetEntity_{ nullptr };

		BulletTrackingTarget target_{};
		BulletTrackingStats stats_{};
	};
}
