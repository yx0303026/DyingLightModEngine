#pragma once

#include "sdk/EngineEntityAbi.h"

#include <cstdint>

class EngineEntityRuntime final
{
public:
	static const DlModEntityApiV1 *GetApi() noexcept;
	static void Shutdown() noexcept;
	/* 内部使用：返回缓存的本地实体实例，必须在游戏线程调用。 */
	static void *LocalEntityInstance() noexcept;
	/* 内部使用：读取实体生命值，必须在游戏线程调用。 */
	static bool ReadEntityHealth(const void *entity, float &bodyHealth) noexcept;
	static bool ReadEntityHealthDetails(const void *entity, float &bodyHealth, float &bloodMaskHealth, std::uint64_t &lastDamagerPlayerNetId) noexcept;
	/* 实体服务：供实体类复用运行时绑定与字段读取逻辑。 */
	static bool IsPlayerInstance(const void *entity) noexcept;
	static bool IsHunterInstance(const void *entity) noexcept;
	static bool IsHumanPlayerInstance(const void *entity) noexcept;
	static bool IsHumanVisualInstance(const void *entity) noexcept;
	static bool IsZombieInstance(const void *entity) noexcept;
	static std::uint8_t EntityModelId(const void *entity) noexcept;
	static DlModEntityKindV1 EntityKind(const void *entity) noexcept;
	static std::uint32_t EntityClassFlags(const void *entity) noexcept;
	static DlModResult ReadSnapshotForInstance(const void *entity, DlModEntitySnapshotV1 *snapshot) noexcept;
	static DlModResult ReadPlayerSnapshot(const void *entity, DlModPlayerSnapshotV1 *snapshot) noexcept;
	static DlModResult ReadHumanSnapshot(const void *entity, DlModHumanSnapshotV1 *snapshot) noexcept;
	static DlModResult ReadHunterSnapshot(const void *entity, DlModHunterSnapshotV1 *snapshot) noexcept;
	static DlModResult SetEntityPosition(const void *entity, const DlModVector3V1 *position) noexcept;
	static DlModResult SetEntityOrientation(const void *entity, const DlModVector3V1 *forward, const DlModVector3V1 *up) noexcept;
	static DlModResult ReadEntityBonePosition(const void *entity, std::uint8_t boneId, DlModVector3V1 *position) noexcept;
	static void *ControlObjectForInstance(const void *entity) noexcept;
	static bool ReadPositionForInstance(const void *entity, DlModVector3V1 &position) noexcept;
	static bool ReadOrientationForInstance(const void *entity, DlModVector3V1 &forward, DlModVector3V1 &up) noexcept;
	static bool ReadModelObjectForInstance(const void *entity, void *&model) noexcept;
	static DlModResult MODSDK_CALL EnumerateNearby(
		const DlModVector3V1 *center,
		float radius,
		DlModNearbyEntityV1 *entities,
		uint32_t capacity,
		uint32_t *count) noexcept;
	static DlModResult MODSDK_CALL ReadBoneByName(
		const DlModEntityV1 *entity,
		const char *name,
		DlModVector3V1 *position) noexcept;
	/* 准星拾取：单次调用内完成枚举、预筛与骨骼精算，必须在游戏线程调用。 */
	static DlModResult MODSDK_CALL PickAimTarget(
		const DlModAimPickOptionsV1 *options,
		DlModAimPickResultV1 *result) noexcept;
	static DlModResult MODSDK_CALL ResolveBoneId(
		const DlModEntityV1 *entity,
		const char *name,
		uint8_t *boneId) noexcept;
	static DlModResult MODSDK_CALL ReadBoneById(
		const DlModEntityV1 *entity,
		uint8_t boneId,
		DlModVector3V1 *position) noexcept;

private:
	static DlModResult MODSDK_CALL GetLocalEntity(DlModEntityV1 *entity) noexcept;
	static DlModResult MODSDK_CALL GetLocalPlayer(DlModPlayerV1 *player) noexcept;
	static DlModResult MODSDK_CALL GetLocalHuman(DlModHumanV1 *human) noexcept;
	static DlModResult MODSDK_CALL GetCurrentHunter(DlModHunterV1 *hunter) noexcept;
	static DlModResult MODSDK_CALL ReadEntity(
		const DlModEntityV1 *entity,
		DlModEntitySnapshotV1 *snapshot) noexcept;
	static DlModResult MODSDK_CALL ReadPlayer(
		const DlModPlayerV1 *player,
		DlModPlayerSnapshotV1 *snapshot) noexcept;
	static DlModResult MODSDK_CALL ReadHuman(
		const DlModHumanV1 *human,
		DlModHumanSnapshotV1 *snapshot) noexcept;
	static DlModResult MODSDK_CALL ReadHunter(
		const DlModHunterV1 *hunter,
		DlModHunterSnapshotV1 *snapshot) noexcept;
	static DlModResult MODSDK_CALL SetPosition(
		const DlModEntityV1 *entity,
		const DlModVector3V1 *position) noexcept;
	static DlModResult MODSDK_CALL SetOrientation(
		const DlModEntityV1 *entity,
		const DlModVector3V1 *forward,
		const DlModVector3V1 *up) noexcept;
	static DlModResult MODSDK_CALL ReadBonePosition(
		const DlModHunterV1 *hunter,
		uint8_t boneId,
		DlModVector3V1 *position) noexcept;
	static DlModResult MODSDK_CALL ReadHunterMotion(
		const DlModHunterV1 *hunter,
		DlModHunterMotionV1 *motion) noexcept;
	static DlModResult MODSDK_CALL InstallHunterTracking() noexcept;
	static uint8_t MODSDK_CALL IsHunterTrackingInstalled() noexcept;
	static DlModResult MODSDK_CALL InstallUvShieldTracking() noexcept;
	static uint8_t MODSDK_CALL IsUvShieldTrackingInstalled() noexcept;
	static DlModResult MODSDK_CALL ReadHunterUvShield(
		DlModHunterUvShieldV1 *snapshot) noexcept;
	static DlModResult MODSDK_CALL SetHunterUvShield(
		uint32_t mode,
		float value) noexcept;
	static DlModResult MODSDK_CALL SetHunterUvShieldCharges(
		int32_t charges) noexcept;
	static DlModResult MODSDK_CALL SetHunterUvShieldDuration(
		float seconds) noexcept;
	static DlModResult MODSDK_CALL SetHunterUvShieldProgress(
		float ratio) noexcept;
	static DlModResult MODSDK_CALL RegisterHunterUvShieldHooks(
		const DlModHunterUvShieldHookDescV1 *desc) noexcept;
	static DlModResult MODSDK_CALL UnregisterHunterUvShieldHooks() noexcept;
	static DlModResult MODSDK_CALL InstallDamageTracking() noexcept;
	static uint8_t MODSDK_CALL IsDamageTrackingInstalled() noexcept;
	static DlModResult MODSDK_CALL RegisterDamageHook(
		ModEngineDamagePreHookFn pre,
		ModEngineDamagePostHookFn post) noexcept;
	static DlModResult MODSDK_CALL UnregisterDamageHook(
		ModEngineDamagePreHookFn pre,
		ModEngineDamagePostHookFn post) noexcept;
};
