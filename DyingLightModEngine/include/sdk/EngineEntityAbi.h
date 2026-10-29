#pragma once

#include "sdk/EngineAbiTypes.h"

/* 实体 ABI 概览：定义实体句柄、稳定快照以及实体和猎手查询函数表。 */
#ifdef __cplusplus
extern "C"
{
#endif

#define DLMOD_ENTITY_HAS_POSITION 0x00000001u
#define DLMOD_ENTITY_HAS_ORIENTATION 0x00000002u
#define DLMOD_ENTITY_HAS_HEALTH 0x00000004u
#define DLMOD_ENTITY_HAS_REPLICATION_ID 0x00000008u
#define DLMOD_ENTITY_HAS_LEVEL 0x00000010u
#define DLMOD_HUNTER_HAS_POSITION 0x00000001u
#define DLMOD_HUNTER_HAS_VELOCITY 0x00000002u
#define DLMOD_HUNTER_HAS_ACCELERATION 0x00000004u
#define DLMOD_HUNTER_HAS_SKELETON_POSITION 0x00000008u
	typedef enum DlModEntityKindV1
	{
		DLMOD_ENTITY_KIND_UNKNOWN = 0,
		DLMOD_ENTITY_KIND_ENTITY = 1,
		DLMOD_ENTITY_KIND_PLAYER = 2,
		DLMOD_ENTITY_KIND_HUMAN = 3,
		DLMOD_ENTITY_KIND_HUNTER = 4
	} DlModEntityKindV1;

	typedef struct DlModHunterMotionV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t validFields;
		uint32_t reserved;
		DlModVector3V1 position;
		DlModVector3V1 velocity;
		DlModVector3V1 acceleration;
		DlModVector3V1 skeletonTransformPosition;
		double positionTimeSeconds;
		double velocityTimeSeconds;
		double accelerationTimeSeconds;
		double skeletonTransformPositionTimeSeconds;
		double readTimeSeconds;
	} DlModHunterMotionV1;

	/* 实体句柄仅持有游戏对象指针；派生句柄使用基础句柄前缀以支持向上转型。 */
	/* C++ 使用继承表达类型关系；C 使用相同布局的嵌套结构体。 */
#if defined(__cplusplus)
	struct DlModEntityV1 { void *instance; };
	struct DlModPlayerV1 : DlModEntityV1 {};
	struct DlModHumanV1 : DlModPlayerV1 {};
	struct DlModHunterV1 : DlModPlayerV1 {};

	struct DlModEntitySnapshotV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t validFields;
		uint32_t kind;
		uint32_t reserved;
		DlModVector3V1 position;
		DlModVector3V1 forward;
		DlModVector3V1 up;
		void *controlObject;
		void *level;
		uint16_t replicationId;
		uint8_t modelId;
		uint8_t reserved2;
		float bodyHealth;
		float bloodMaskHealth;
		uint64_t lastDamagerPlayerNetId;
	};
	struct DlModPlayerSnapshotV1 : DlModEntitySnapshotV1 { uint32_t playerFlags; };
	struct DlModHumanSnapshotV1 : DlModPlayerSnapshotV1 { uint32_t humanFlags; };
	struct DlModHunterSnapshotV1 : DlModPlayerSnapshotV1 { DlModHunterMotionV1 motion; };
#else
	typedef struct DlModEntityV1 { void *instance; } DlModEntityV1;
	typedef struct DlModPlayerV1 { DlModEntityV1 entity; } DlModPlayerV1;
	typedef struct DlModHumanV1 { DlModPlayerV1 player; } DlModHumanV1;
	typedef struct DlModHunterV1 { DlModPlayerV1 player; } DlModHunterV1;

	typedef struct DlModEntitySnapshotV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t validFields;
		uint32_t kind;
		uint32_t reserved;
		DlModVector3V1 position;
		DlModVector3V1 forward;
		DlModVector3V1 up;
		void *controlObject;
		void *level;
		uint16_t replicationId;
		uint8_t modelId;
		uint8_t reserved2;
		float bodyHealth;
		float bloodMaskHealth;
		uint64_t lastDamagerPlayerNetId;
	} DlModEntitySnapshotV1;
	typedef struct DlModPlayerSnapshotV1 { DlModEntitySnapshotV1 entity; uint32_t playerFlags; } DlModPlayerSnapshotV1;
	typedef struct DlModHumanSnapshotV1 { DlModPlayerSnapshotV1 player; uint32_t humanFlags; } DlModHumanSnapshotV1;
	typedef struct DlModHunterSnapshotV1 { DlModPlayerSnapshotV1 player; DlModHunterMotionV1 motion; } DlModHunterSnapshotV1;
#endif

/* 派生快照以基础快照为前缀，调用方可按实际类型读取扩展字段。 */
#define DLMOD_ENTITY_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModEntitySnapshotV1))
#define DLMOD_PLAYER_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModPlayerSnapshotV1))
#define DLMOD_HUMAN_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModHumanSnapshotV1))
#define DLMOD_HUNTER_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModHunterSnapshotV1))

	/* 本地实体查询、读取和写入应在游戏线程调用；猎手运动快照允许跨线程读取。 */
	typedef DlModResult(MODSDK_CALL *ModEngineGetLocalEntityFn)(DlModEntityV1 *entity);
	typedef DlModResult(MODSDK_CALL *ModEngineGetLocalPlayerFn)(DlModPlayerV1 *player);
	typedef DlModResult(MODSDK_CALL *ModEngineGetLocalHumanFn)(DlModHumanV1 *human);
	typedef DlModResult(MODSDK_CALL *ModEngineGetCurrentHunterFn)(DlModHunterV1 *hunter);
	typedef DlModResult(MODSDK_CALL *ModEngineReadEntityFn)(
		const DlModEntityV1 *entity,
		DlModEntitySnapshotV1 *snapshot);
	typedef DlModResult(MODSDK_CALL *ModEngineReadPlayerFn)(
		const DlModPlayerV1 *player,
		DlModPlayerSnapshotV1 *snapshot);
	typedef DlModResult(MODSDK_CALL *ModEngineReadHumanFn)(
		const DlModHumanV1 *human,
		DlModHumanSnapshotV1 *snapshot);
	typedef DlModResult(MODSDK_CALL *ModEngineReadHunterFn)(
		const DlModHunterV1 *hunter,
		DlModHunterSnapshotV1 *snapshot);
	typedef DlModResult(MODSDK_CALL *ModEngineSetEntityPositionFn)(
		const DlModEntityV1 *entity,
		const DlModVector3V1 *position);
	typedef DlModResult(MODSDK_CALL *ModEngineSetEntityOrientationFn)(
		const DlModEntityV1 *entity,
		const DlModVector3V1 *forward,
		const DlModVector3V1 *up);
	typedef DlModResult(MODSDK_CALL *ModEngineReadBonePositionFn)(
		const DlModHunterV1 *hunter,
		uint8_t boneId,
		DlModVector3V1 *position);
	typedef DlModResult(MODSDK_CALL *ModEngineReadHunterMotionFn)(
		const DlModHunterV1 *hunter,
		DlModHunterMotionV1 *motion);
	typedef DlModResult(MODSDK_CALL *ModEngineInstallHunterTrackingFn)(void);
	typedef uint8_t(MODSDK_CALL *ModEngineIsHunterTrackingInstalledFn)(void);

	typedef struct DlModEntityApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		ModEngineGetLocalEntityFn getLocalEntity;
		ModEngineGetLocalPlayerFn getLocalPlayer;
		ModEngineGetLocalHumanFn getLocalHuman;
		ModEngineGetCurrentHunterFn getCurrentHunter;
		ModEngineReadEntityFn readEntity;
		ModEngineReadPlayerFn readPlayer;
		ModEngineReadHumanFn readHuman;
		ModEngineReadHunterFn readHunter;
		ModEngineSetEntityPositionFn setPosition;
		ModEngineSetEntityOrientationFn setOrientation;
		ModEngineReadBonePositionFn readBonePosition;
		ModEngineReadHunterMotionFn readHunterMotion;
		ModEngineInstallHunterTrackingFn installHunterTracking;
		ModEngineIsHunterTrackingInstalledFn isHunterTrackingInstalled;
	} DlModEntityApiV1;

#define DLMOD_ENTITY_API_V1_MIN_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, getLocalEntity) + sizeof(ModEngineGetLocalEntityFn)))
#define DLMOD_ENTITY_API_V1_HUNTER_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, isHunterTrackingInstalled) + sizeof(ModEngineIsHunterTrackingInstalledFn)))

#ifdef __cplusplus
}
#endif
