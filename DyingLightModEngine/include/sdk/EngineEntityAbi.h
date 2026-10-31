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
#define DLMOD_HUNTER_UV_HAS_STATE 0x00000001u
#define DLMOD_HUNTER_UV_HAS_SHIELD 0x00000002u
#define DLMOD_HUNTER_UV_HAS_FLAG 0x00000004u
#define DLMOD_HUNTER_UV_HAS_CHARGES 0x00000008u
#define DLMOD_DAMAGE_FLAG_PRE 0x00000001u
#define DLMOD_DAMAGE_FLAG_POST 0x00000002u
#define DLMOD_DAMAGE_FLAG_BLOCKED 0x00000004u
#define DLMOD_DAMAGE_FLAG_VICTIM_PLAYER 0x00000008u
#define DLMOD_DAMAGE_FLAG_GAME_APPLIED 0x00000010u
	typedef enum DlModEntityKindV1
	{
		DLMOD_ENTITY_KIND_UNKNOWN = 0,
		DLMOD_ENTITY_KIND_ENTITY = 1,
		DLMOD_ENTITY_KIND_PLAYER = 2,
		DLMOD_ENTITY_KIND_HUMAN = 3,
		DLMOD_ENTITY_KIND_HUNTER = 4,
		DLMOD_ENTITY_KIND_ZOMBIE = 5
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

	/* 紫外线护盾写入模式：GAME 不干预，PIN 固定剩余秒数，OFF 清零，SCALE 按倍率缩放。 */
#define DLMOD_HUNTER_UV_SHIELD_MODE_GAME 0u
#define DLMOD_HUNTER_UV_SHIELD_MODE_PIN 1u
#define DLMOD_HUNTER_UV_SHIELD_MODE_OFF 2u
#define DLMOD_HUNTER_UV_SHIELD_MODE_SCALE 3u

	/* 紫外线护盾生命周期来源：烟雾、捶地、主端更新、客户端更新。 */
#define DLMOD_HUNTER_UV_HOOK_SOURCE_SMOKE 1u
#define DLMOD_HUNTER_UV_HOOK_SOURCE_GROUND_POUND 2u
#define DLMOD_HUNTER_UV_HOOK_SOURCE_UPDATE 3u
#define DLMOD_HUNTER_UV_HOOK_SOURCE_CLIENT_UPDATE 4u

	/* 开始钩子返回非零继续执行，返回零则跳过本次护盾提交。 */
	typedef uint8_t(MODSDK_CALL *ModEngineHunterUvShieldStartHookFn)(
		void *stateMachine,
		uint32_t source,
		float durationSeconds,
		void *userData);
	typedef void(MODSDK_CALL *ModEngineHunterUvShieldUpdateHookFn)(
		void *stateMachine,
		uint32_t source,
		float previousSeconds,
		float remainingSeconds,
		void *userData);
	typedef void(MODSDK_CALL *ModEngineHunterUvShieldEndHookFn)(
		void *stateMachine,
		uint32_t source,
		void *userData);

	typedef struct DlModHunterUvShieldHookDescV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		void *userData;
		ModEngineHunterUvShieldStartHookFn onStart;
		ModEngineHunterUvShieldUpdateHookFn onUpdate;
		ModEngineHunterUvShieldEndHookFn onEnd;
	} DlModHunterUvShieldHookDescV1;

	typedef DlModResult(MODSDK_CALL *ModEngineRegisterHunterUvShieldHooksFn)(
		const DlModHunterUvShieldHookDescV1 *desc);
	typedef DlModResult(MODSDK_CALL *ModEngineUnregisterHunterUvShieldHooksFn)(void);
	typedef DlModResult(MODSDK_CALL *ModEngineSetHunterUvShieldTimerFn)(
		void *stateMachine,
		float seconds);

	/* 紫外线恢复钩子：状态 9 写入前触发开始回调，状态 11 写入前触发完成回调。
	   开始回调返回非零继续执行，返回零则跳过状态 9 的写入。 */
	typedef uint8_t(MODSDK_CALL *ModEngineHunterUvRecoveryStartHookFn)(
		void *stateMachine,
		uint32_t newState,
		void *userData);
	typedef void(MODSDK_CALL *ModEngineHunterUvRecoveryEndHookFn)(
		void *stateMachine,
		uint32_t previousState,
		void *userData);

	typedef struct DlModHunterUvRecoveryHookDescV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		void *userData;
		ModEngineHunterUvRecoveryStartHookFn onStart;
		ModEngineHunterUvRecoveryEndHookFn onEnd;
	} DlModHunterUvRecoveryHookDescV1;

	typedef DlModResult(MODSDK_CALL *ModEngineRegisterHunterUvRecoveryHooksFn)(
		const DlModHunterUvRecoveryHookDescV1 *desc);
	typedef DlModResult(MODSDK_CALL *ModEngineUnregisterHunterUvRecoveryHooksFn)(void);
	typedef DlModResult(MODSDK_CALL *ModEngineInstallUvRecoveryTrackingFn)(void);
	typedef uint8_t(MODSDK_CALL *ModEngineIsUvRecoveryTrackingInstalledFn)(void);

	/* 紫外线护盾快照：shieldSeconds 为状态机上的光免疫剩余秒数，shieldFlag 为 HUD 紫外格挡标记，
	   smokeCharges 为防御烟雾剩余充能（状态机 +0x70），shieldDuration 为最近一次激活的时长。 */
	typedef struct DlModHunterUvShieldV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t validFields;
		uint32_t zombieState;
		float shieldSeconds;
		float shieldDuration;
		uint32_t shieldFlag;
		int32_t smokeCharges;
		void *stateMachine;
		double readTimeSeconds;
	} DlModHunterUvShieldV1;

	/* 受伤事件：healthModule 为结算伤害的 CHealthModule，entity 为其宿主实体，
	   instigator 为造成伤害的实体（可能为空，如环境伤害）。
	   incomingDamage 是本次结算收到的原始伤害；damage 可被受伤前钩子改写，
	   改写后的值会交给原版 HealthModule::AddDamageToBody 继续处理。
	   appliedDamage 为受伤后钩子中的实际结算值（游戏返回值）。 */
	typedef struct DlModDamageEventV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t flags;
		uint32_t damageType;
		void *healthModule;
		void *entity;
		void *instigator;
		float incomingDamage;
		float damage;
		float appliedDamage;
		float healthBefore;
		float healthAfter;
		uint64_t sequence;
	} DlModDamageEventV1;

	/* 受伤前钩子：返回非 0 继续结算，返回 0 阻止本次受伤（原版结算被跳过）。 */
	typedef uint8_t(MODSDK_CALL *ModEngineDamagePreHookFn)(DlModDamageEventV1 *event);
	typedef void(MODSDK_CALL *ModEngineDamagePostHookFn)(const DlModDamageEventV1 *event);

	/* 实体句柄仅持有游戏对象指针；派生句柄使用基础句柄前缀以支持向上转型。 */
	/* C++ 使用继承表达类型关系；C 使用相同布局的嵌套结构体。 */
#if defined(__cplusplus)
	struct DlModEntityV1
	{
		void *instance;
	};
	struct DlModPlayerV1 : DlModEntityV1
	{
	};
	struct DlModHumanV1 : DlModPlayerV1
	{
	};
	struct DlModHunterV1 : DlModPlayerV1
	{
	};

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
	struct DlModPlayerSnapshotV1 : DlModEntitySnapshotV1
	{
		uint32_t playerFlags;
	};
	struct DlModHumanSnapshotV1 : DlModPlayerSnapshotV1
	{
		uint32_t humanFlags;
	};
	struct DlModHunterSnapshotV1 : DlModPlayerSnapshotV1
	{
		DlModHunterMotionV1 motion;
	};
#else
typedef struct DlModEntityV1
{
	void *instance;
} DlModEntityV1;
typedef struct DlModPlayerV1
{
	DlModEntityV1 entity;
} DlModPlayerV1;
typedef struct DlModHumanV1
{
	DlModPlayerV1 player;
} DlModHumanV1;
typedef struct DlModHunterV1
{
	DlModPlayerV1 player;
} DlModHunterV1;

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
typedef struct DlModPlayerSnapshotV1
{
	DlModEntitySnapshotV1 entity;
	uint32_t playerFlags;
} DlModPlayerSnapshotV1;
typedef struct DlModHumanSnapshotV1
{
	DlModPlayerSnapshotV1 player;
	uint32_t humanFlags;
} DlModHumanSnapshotV1;
typedef struct DlModHunterSnapshotV1
{
	DlModPlayerSnapshotV1 player;
	DlModHunterMotionV1 motion;
} DlModHunterSnapshotV1;
#endif

/* 派生快照以基础快照为前缀，调用方可按实际类型读取扩展字段。 */
#define DLMOD_ENTITY_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModEntitySnapshotV1))
#define DLMOD_PLAYER_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModPlayerSnapshotV1))
#define DLMOD_HUMAN_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModHumanSnapshotV1))
#define DLMOD_HUNTER_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModHunterSnapshotV1))

	/* 附近实体快照：kind 使用 DLMOD_ENTITY_KIND_*，position 由 validFields 标记。
	   容量不足时优先返回角色（PLAYER/HUMAN/HUNTER/ZOMBIE），再补普通实体。 */
	typedef struct DlModNearbyEntityV1
	{
		uint32_t structSize;
		uint32_t kind;
		uint32_t validFields;
		uint32_t reserved;
		uint64_t identity;
		DlModVector3V1 position;
		DlModEntityV1 entity;
	} DlModNearbyEntityV1;

#define DLMOD_NEARBY_ENTITY_V1_BASE_SIZE \
	((uint32_t)offsetof(DlModNearbyEntityV1, entity))

	/* 准星拾取的骨骼目标：BODY 取躯干，HEAD 取头部。 */
#define DLMOD_AIM_BONE_BODY 0u
#define DLMOD_AIM_BONE_HEAD 1u
	/* 允许拾取的实体种类位掩码，位序与 DlModEntityKindV1 一致。 */
#define DLMOD_AIM_KIND_MASK(kind) (1u << (kind))
#define DLMOD_AIM_KIND_DEFAULT                       \
	(DLMOD_AIM_KIND_MASK(DLMOD_ENTITY_KIND_PLAYER) | \
	 DLMOD_AIM_KIND_MASK(DLMOD_ENTITY_KIND_HUMAN) |  \
	 DLMOD_AIM_KIND_MASK(DLMOD_ENTITY_KIND_HUNTER) | \
	 DLMOD_AIM_KIND_MASK(DLMOD_ENTITY_KIND_ZOMBIE))

	/* 准星拾取参数：一次调用内完成角色枚举、屏幕空间预筛与骨骼精算。
	   center 为零向量时使用当前相机位置。 */
	typedef struct DlModAimPickOptionsV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t boneTarget;
		uint32_t allowedKinds;
		DlModVector3V1 center;
		float radius;
		float viewportWidth;
		float viewportHeight;
		/* 屏幕中心裁剪半径（像素），<= 0 表示不裁剪。 */
		float fovRadius;
		/* 预筛后最多精算的候选数，0 表示引擎默认值。 */
		uint32_t maxBoneReads;
		uint32_t reserved;
	} DlModAimPickOptionsV1;

#define DLMOD_AIM_PICK_HAS_TARGET 0x00000001u
#define DLMOD_AIM_PICK_HAS_BONE 0x00000002u

	/* 准星拾取结果：boneId 可直接用于 readBoneById 做逐帧轻量追踪。 */
	typedef struct DlModAimPickResultV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t validFields;
		uint32_t kind;
		DlModEntityV1 entity;
		DlModVector3V1 position;
		float screenX;
		float screenY;
		uint32_t boneId;
		/* 诊断计数：参与分类的对象数、通过的候选数、实际精算数。 */
		uint32_t scanned;
		uint32_t candidates;
		uint32_t refined;
		/* 1 表示本次复用了缓存的角色列表，未重新做空间查询。 */
		uint32_t cached;
	} DlModAimPickResultV1;

#define DLMOD_AIM_PICK_OPTIONS_V1_SIZE ((uint32_t)sizeof(DlModAimPickOptionsV1))
#define DLMOD_AIM_PICK_RESULT_V1_SIZE ((uint32_t)sizeof(DlModAimPickResultV1))

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
	/* 枚举以 center 为球心、radius 为半径的活动实体；entities 指向调用方存储区。 */
	typedef DlModResult(MODSDK_CALL *ModEngineEnumerateNearbyFn)(
		const DlModVector3V1 *center,
		float radius,
		DlModNearbyEntityV1 *entities,
		uint32_t capacity,
		uint32_t *count);
	/* 按骨骼名读取世界坐标，适用于任意拥有 CModelObject 的实体。 */
	typedef DlModResult(MODSDK_CALL *ModEngineReadBoneByNameFn)(
		const DlModEntityV1 *entity,
		const char *name,
		DlModVector3V1 *position);
	/* 准星目标拾取：单次调用完成枚举、投影预筛与骨骼精算，供逐帧瞄准使用。 */
	typedef DlModResult(MODSDK_CALL *ModEnginePickAimTargetFn)(
		const DlModAimPickOptionsV1 *options,
		DlModAimPickResultV1 *result);
	/* 骨骼名解析：名字只查一次表，之后用 ID 重复读取。 */
	typedef DlModResult(MODSDK_CALL *ModEngineResolveBoneIdFn)(
		const DlModEntityV1 *entity,
		const char *name,
		uint8_t *boneId);
	/* 按骨骼 ID 读取关节世界坐标，避免每次读取都做字符串查找。 */
	typedef DlModResult(MODSDK_CALL *ModEngineReadBoneByIdFn)(
		const DlModEntityV1 *entity,
		uint8_t boneId,
		DlModVector3V1 *position);
	/* 紫外线护盾：安装状态机写入钩子、读取当前护盾状态并下发覆盖策略。 */
	typedef DlModResult(MODSDK_CALL *ModEngineInstallUvShieldTrackingFn)(void);
	typedef uint8_t(MODSDK_CALL *ModEngineIsUvShieldTrackingInstalledFn)(void);
	typedef DlModResult(MODSDK_CALL *ModEngineReadHunterUvShieldFn)(
		DlModHunterUvShieldV1 *snapshot);
	typedef DlModResult(MODSDK_CALL *ModEngineSetHunterUvShieldFn)(
		uint32_t mode,
		float value);
	/* 充能：charges < 0 关闭覆盖，否则每帧把状态机剩余充能固定为该值。
	   时长：seconds <= 0 关闭覆盖，否则激活时直接用该秒数替换游戏配置值。 */
	typedef DlModResult(MODSDK_CALL *ModEngineSetHunterUvShieldChargesFn)(
		int32_t charges);
	typedef DlModResult(MODSDK_CALL *ModEngineSetHunterUvShieldDurationFn)(
		float seconds);
	typedef DlModResult(MODSDK_CALL *ModEngineSetHunterUvShieldProgressFn)(
		float ratio);
	/* 受伤回调：安装两个 HealthModule::AddDamageToBody 跳板，并按需注册前置/后置回调。 */
	typedef DlModResult(MODSDK_CALL *ModEngineInstallDamageTrackingFn)(void);
	typedef uint8_t(MODSDK_CALL *ModEngineIsDamageTrackingInstalledFn)(void);
	typedef DlModResult(MODSDK_CALL *ModEngineRegisterDamageHookFn)(
		ModEngineDamagePreHookFn pre,
		ModEngineDamagePostHookFn post);
	typedef DlModResult(MODSDK_CALL *ModEngineUnregisterDamageHookFn)(
		ModEngineDamagePreHookFn pre,
		ModEngineDamagePostHookFn post);

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
		ModEngineEnumerateNearbyFn enumerateNearby;
		ModEngineReadBoneByNameFn readBoneByName;
		ModEngineInstallUvShieldTrackingFn installUvShieldTracking;
		ModEngineIsUvShieldTrackingInstalledFn isUvShieldTrackingInstalled;
		ModEngineReadHunterUvShieldFn readHunterUvShield;
		ModEngineSetHunterUvShieldFn setHunterUvShield;
		ModEngineSetHunterUvShieldChargesFn setHunterUvShieldCharges;
		ModEngineSetHunterUvShieldDurationFn setHunterUvShieldDuration;
		ModEngineSetHunterUvShieldProgressFn setHunterUvShieldProgress;
		ModEngineRegisterHunterUvShieldHooksFn registerHunterUvShieldHooks;
		ModEngineUnregisterHunterUvShieldHooksFn unregisterHunterUvShieldHooks;
		ModEngineSetHunterUvShieldTimerFn setHunterUvShieldTimer;
		ModEngineInstallDamageTrackingFn installDamageTracking;
		ModEngineIsDamageTrackingInstalledFn isDamageTrackingInstalled;
		ModEngineRegisterDamageHookFn registerDamageHook;
		ModEngineUnregisterDamageHookFn unregisterDamageHook;
		/* 以下为向后兼容追加项，调用前请按 structSize 校验。 */
		ModEnginePickAimTargetFn pickAimTarget;
		ModEngineResolveBoneIdFn resolveBoneId;
		ModEngineReadBoneByIdFn readBoneById;
		ModEngineInstallUvRecoveryTrackingFn installUvRecoveryTracking;
		ModEngineIsUvRecoveryTrackingInstalledFn isUvRecoveryTrackingInstalled;
		ModEngineRegisterHunterUvRecoveryHooksFn registerHunterUvRecoveryHooks;
		ModEngineUnregisterHunterUvRecoveryHooksFn unregisterHunterUvRecoveryHooks;
	} DlModEntityApiV1;

#define DLMOD_ENTITY_API_V1_MIN_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, getLocalEntity) + sizeof(ModEngineGetLocalEntityFn)))
#define DLMOD_ENTITY_API_V1_HUNTER_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, isHunterTrackingInstalled) + sizeof(ModEngineIsHunterTrackingInstalledFn)))
#define DLMOD_ENTITY_API_V1_NEARBY_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, enumerateNearby) + sizeof(ModEngineEnumerateNearbyFn)))
#define DLMOD_ENTITY_API_V1_BONE_NAME_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, readBoneByName) + sizeof(ModEngineReadBoneByNameFn)))
#define DLMOD_ENTITY_API_V1_UV_SHIELD_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, setHunterUvShieldProgress) + sizeof(ModEngineSetHunterUvShieldProgressFn)))
#define DLMOD_ENTITY_API_V1_UV_HOOK_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, unregisterHunterUvShieldHooks) + sizeof(ModEngineUnregisterHunterUvShieldHooksFn)))
#define DLMOD_ENTITY_API_V1_UV_TIMER_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, setHunterUvShieldTimer) + sizeof(ModEngineSetHunterUvShieldTimerFn)))
#define DLMOD_ENTITY_API_V1_UV_RECOVERY_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, unregisterHunterUvRecoveryHooks) + sizeof(ModEngineUnregisterHunterUvRecoveryHooksFn)))
#define DLMOD_ENTITY_API_V1_DAMAGE_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, unregisterDamageHook) + sizeof(ModEngineUnregisterDamageHookFn)))
#define DLMOD_ENTITY_API_V1_AIM_SIZE \
	((uint32_t)(offsetof(DlModEntityApiV1, readBoneById) + sizeof(ModEngineReadBoneByIdFn)))

#ifdef __cplusplus
}
#endif
