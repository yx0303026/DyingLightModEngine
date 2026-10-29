#pragma once

#include "sdk/EngineAbiTypes.h"

/* 弹丸 ABI 概览：暴露 BulletManager 更新 Hook、帧回调与记录读写原语。
   弹道追踪、目标锁定等玩法逻辑由模组在回调内实现，引擎只提供 Hook 与数据访问。 */
#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum DlModProjectileKindV1
	{
		DLMOD_PROJECTILE_KIND_UNKNOWN = 0,
		DLMOD_PROJECTILE_KIND_ARROW = 1,
		DLMOD_PROJECTILE_KIND_BOLT = 2,
		DLMOD_PROJECTILE_KIND_BULLET = 3
	} DlModProjectileKindV1;

#define DLMOD_PROJECTILE_UPDATE_NEW_FLIGHT 0x00000001u

/* 弹丸类别过滤位掩码，便于模组用单个字段保存开关状态。 */
#define DLMOD_PROJECTILE_FILTER_ARROW 0x00000001u
#define DLMOD_PROJECTILE_FILTER_BOLT 0x00000002u
#define DLMOD_PROJECTILE_FILTER_BULLET 0x00000004u
#define DLMOD_PROJECTILE_FILTER_DEFAULT 0x00000007u

#define DLMOD_PROJECTILE_KINEMATICS_FINISHED 0x00000001u
#define DLMOD_PROJECTILE_KINEMATICS_HAS_NEXT 0x00000002u

	typedef struct DlModProjectileKinematicsV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t flags;
		uint32_t reserved;
		DlModVector3V1 previous;
		DlModVector3V1 current;
		DlModVector3V1 next;
	} DlModProjectileKinematicsV1;

	/* record 是不透明弹丸记录指针，只在对应的更新回调内有效。 */
	typedef struct DlModProjectileUpdateV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t kind;
		uint32_t flags;
		void *record;
		float deltaSeconds;
		uint32_t reserved;
	} DlModProjectileUpdateV1;

	/* 更新回调在原始 BulletManager 更新之前触发，运行于游戏线程。 */
	typedef void(MODSDK_CALL *DlModProjectileUpdateCallback)(
		void *context,
		const DlModProjectileUpdateV1 *update);
	/* 帧回调在场景帧更新之后触发，运行于游戏线程。 */
	typedef void(MODSDK_CALL *DlModFrameCallback)(void *context);

	typedef DlModResult(MODSDK_CALL *ModEngineInstallProjectileHookFn)(void);
	typedef uint8_t(MODSDK_CALL *ModEngineIsProjectileHookInstalledFn)(void);
	typedef void(MODSDK_CALL *ModEngineSetProjectileUpdateCallbackFn)(
		void *context,
		DlModProjectileUpdateCallback callback);
	typedef void(MODSDK_CALL *ModEngineSetFrameCallbackFn)(
		void *context,
		DlModFrameCallback callback);
	typedef uint32_t(MODSDK_CALL *ModEngineClassifyProjectileFn)(const void *record);
	typedef DlModResult(MODSDK_CALL *ModEngineReadProjectileKinematicsFn)(
		const void *record,
		DlModProjectileKinematicsV1 *kinematics);
	typedef DlModResult(MODSDK_CALL *ModEngineReadProjectilePositionFn)(
		const void *record,
		DlModVector3V1 *position);
	typedef DlModResult(MODSDK_CALL *ModEngineReadProjectileVelocityFn)(
		const void *record,
		DlModVector3V1 *velocity);
	typedef DlModResult(MODSDK_CALL *ModEngineWriteProjectileVelocityFn)(
		void *record,
		const DlModVector3V1 *velocity);

	typedef struct DlModProjectileManagerApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		ModEngineInstallProjectileHookFn install;
		ModEngineIsProjectileHookInstalledFn isInstalled;
		ModEngineSetProjectileUpdateCallbackFn setUpdateCallback;
		ModEngineSetFrameCallbackFn setFrameCallback;
		ModEngineClassifyProjectileFn classify;
		ModEngineReadProjectileKinematicsFn readKinematics;
		ModEngineReadProjectilePositionFn readPosition;
		ModEngineReadProjectileVelocityFn readVelocity;
		ModEngineWriteProjectileVelocityFn writeVelocity;
	} DlModProjectileManagerApiV1;

#define DLMOD_PROJECTILE_MANAGER_API_V1_MIN_SIZE \
	((uint32_t)(offsetof(DlModProjectileManagerApiV1, install) + sizeof(ModEngineInstallProjectileHookFn)))
#define DLMOD_PROJECTILE_MANAGER_API_V1_WRITE_SIZE \
	((uint32_t)(offsetof(DlModProjectileManagerApiV1, writeVelocity) + sizeof(ModEngineWriteProjectileVelocityFn)))

#ifdef __cplusplus
}
#endif
