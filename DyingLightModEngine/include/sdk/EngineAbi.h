#pragma once

#include <stdint.h>
#include <stddef.h>

/* 平台 ABI 概览：统一调用约定、导入导出属性和模组导出标记。 */
#pragma region Platform ABI
#if defined(_WIN32)
#define MODSDK_CALL __cdecl
#if defined(MODSDK_BUILD)
#define MODSDK_API __declspec(dllexport)
#else
#define MODSDK_API __declspec(dllimport)
#endif
#define MODSDK_MOD_EXPORT __declspec(dllexport)
#else
#define MODSDK_CALL
#define MODSDK_API
#define MODSDK_MOD_EXPORT
#endif
#pragma endregion

#ifdef __cplusplus
extern "C"
{
#endif

/* 核心类型概览：定义 ABI 版本、通用结果码和日志回调。 */
#pragma region Core ABI Types
#define MODSDK_ABI_VERSION 1u

	typedef enum DlModResult
	{
		DLMOD_RESULT_SUCCESS = 0,
		DLMOD_RESULT_INVALID_ARGUMENT = 1,
		DLMOD_RESULT_UNSUPPORTED_ABI = 2,
		DLMOD_RESULT_INVALID_STATE = 3,
		DLMOD_RESULT_NOT_FOUND = 4,
		DLMOD_RESULT_IO_ERROR = 5,
		DLMOD_RESULT_PARTIAL_SUCCESS = 6,
		DLMOD_RESULT_UNSUPPORTED_FEATURE = 7
	} DlModResult;

	typedef void(MODSDK_CALL *EngineLogCallback)(void *context, uint32_t level, const char *message);
#pragma endregion

/* 碰撞 API 概览：定义跨 DLL 使用的向量、命中箱快照和采集函数表。 */
#pragma region Collision API
	typedef struct DlModVector3V1
	{
		float x;
		float y;
		float z;
	} DlModVector3V1;

	typedef struct DlModHitboxV1
	{
		uint32_t structSize;
		uint32_t elementIndex;
		uint32_t flags;
		DlModVector3V1 localMinimum;
		DlModVector3V1 localMaximum;
		/* Row-major 3x4 world transform: three basis rows followed by translation. */
		float worldMatrix[12];
		char name[128];
		DlModVector3V1 worldCenter;
	} DlModHitboxV1;

	typedef struct DlModCollisionLineV1
	{
		DlModVector3V1 start;
		DlModVector3V1 end;
	} DlModCollisionLineV1;

	typedef struct DlModHitboxSnapshotV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t traceMode;
		uint32_t unsupportedCount;
		uint32_t requiredCount;
		uint32_t count;
		uint32_t capacity;
		uint32_t elementSize;
		DlModHitboxV1 *elements;
		/* Optional extended output. Omit when only hitbox parts are needed. */
		uint32_t usesBoxQueries;
		uint32_t wireframeRequiredCount;
		uint32_t wireframeCount;
		uint32_t wireframeCapacity;
		uint32_t shapeCount;
		uint32_t wireframeTruncated;
		DlModCollisionLineV1 *wireframeLines;
	} DlModHitboxSnapshotV1;

#define DLMOD_HITBOX_V1_BASE_SIZE \
	((uint32_t)offsetof(DlModHitboxV1, worldCenter))
#define DLMOD_HITBOX_SNAPSHOT_V1_BASE_SIZE \
	((uint32_t)offsetof(DlModHitboxSnapshotV1, usesBoxQueries))

	/* Capture must run on the game's thread; elements is caller-owned storage. */
	typedef DlModResult(MODSDK_CALL *ModEngineCaptureHitboxesFn)(
		void *controlObject,
		DlModHitboxSnapshotV1 *snapshot);
	typedef DlModResult(MODSDK_CALL *ModEngineGetHitboxByNameFn)(
		void *controlObject,
		const char *name,
		DlModHitboxV1 *hitbox,
		uint32_t hitboxSize);

	typedef struct DlModCollisionApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		ModEngineCaptureHitboxesFn captureHitboxes;
		ModEngineGetHitboxByNameFn getHitboxByName;
	} DlModCollisionApiV1;

#define DLMOD_COLLISION_API_V1_MIN_SIZE \
	((uint32_t)(offsetof(DlModCollisionApiV1, captureHitboxes) + sizeof(ModEngineCaptureHitboxesFn)))
#define DLMOD_COLLISION_API_V1_HITBOX_NAME_SIZE \
	((uint32_t)(offsetof(DlModCollisionApiV1, getHitboxByName) + sizeof(ModEngineGetHitboxByNameFn)))

#pragma endregion

/* 宿主与输入 API 概览：向模组提供日志、碰撞、实体、Hook 和 UI 鼠标能力。 */
#pragma region Host and UI Input API
#define DLMOD_UI_MOUSE_POSITION_VALID 0x1u
#define DLMOD_UI_MOUSE_DEVICE_CAPTURED 0x2u

	/*
	 * Mouse state sampled from the game's DirectInput mouse device while a mod UI
	 * is open. Games that read the mouse through DirectInput keep receiving the
	 * device state even when window messages are filtered, so the engine neutralizes
	 * that device and forwards its relative motion to the UI instead.
	 */
	typedef struct DlModUiMouseStateV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t flags;
		/* Client-space cursor position, valid when DLMOD_UI_MOUSE_POSITION_VALID is set. */
		int32_t positionX;
		int32_t positionY;
		/* Wheel notches accumulated since the previous call. */
		float wheelDelta;
		/* Pressed buttons: bit0 left, bit1 right, bit2 middle, bit3 X1, bit4 X2. */
		uint32_t buttons;
	} DlModUiMouseStateV1;

	/*
	 * Returns 1 while the UI owns the virtual pointer. The device-captured flag
	 * is set when the DirectInput bridge is active; position data can also come
	 * from the window's raw-input fallback.
	 */
	typedef uint8_t(MODSDK_CALL *ModEngineGetUiMouseStateFn)(
		DlModUiMouseStateV1 *state);

	#pragma region Mod Hook API
	/* 模组 Hook API 概览：通过共享目标链注册 detour，并由引擎按模组生命周期清理。 */
	typedef DlModResult(MODSDK_CALL *ModEngineRegisterHookFn)(
		void *target,
		void *detour,
		void **original);

	typedef struct DlModHookApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		ModEngineRegisterHookFn registerHook;
	} DlModHookApiV1;

#define DLMOD_HOOK_API_V1_MIN_SIZE \
	((uint32_t)(offsetof(DlModHookApiV1, registerHook) + sizeof(ModEngineRegisterHookFn)))
	#pragma endregion

	struct DlModEntityApiV1;

	typedef struct DlModHostApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		void *context;
		EngineLogCallback log;
		/* Optional DirectInput mouse bridge; null when the engine predates it. */
		ModEngineGetUiMouseStateFn getUiMouseState;
		/* Optional game collision API; null when unavailable. */
		const DlModCollisionApiV1 *collision;
		/* Optional game entity API; null when unavailable. */
		const struct DlModEntityApiV1 *entity;
		/* Optional shared hook registry; null when unavailable. */
		const DlModHookApiV1 *hooks;
	} DlModHostApiV1;

#define DLMOD_HOST_API_V1_MOUSE_STATE_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, getUiMouseState) + sizeof(ModEngineGetUiMouseStateFn)))
#define DLMOD_HOST_API_V1_COLLISION_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, collision) + sizeof(const DlModCollisionApiV1 *)))
#define DLMOD_HOST_API_V1_ENTITY_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, entity) + sizeof(const DlModEntityApiV1 *)))
#define DLMOD_HOST_API_V1_HOOKS_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, hooks) + sizeof(const DlModHookApiV1 *)))

#pragma endregion

	/* 实体 ABI 概览：为 Entity、Player、Human、Hunter 提供稳定句柄、快照和运行时函数表。 */
#pragma region Entity API
#define DLMOD_ENTITY_HAS_POSITION 0x00000001u
#define DLMOD_ENTITY_HAS_ORIENTATION 0x00000002u
#define DLMOD_ENTITY_HAS_HEALTH 0x00000004u
#define DLMOD_ENTITY_HAS_REPLICATION_ID 0x00000008u
#define DLMOD_ENTITY_HAS_LEVEL 0x00000010u
#define DLMOD_HUNTER_HAS_POSITION 0x00000001u
#define DLMOD_HUNTER_HAS_VELOCITY 0x00000002u
#define DLMOD_HUNTER_HAS_ACCELERATION 0x00000004u
#define DLMOD_HUNTER_HAS_SKELETON_POSITION 0x00000008u

	/* Entity 快照中的 controlObject 可直接传给碰撞 API；原生对象不由模组释放。 */
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

	/* 实体句柄仅持有游戏对象指针；派生句柄沿用基类前缀，便于安全向上转型。 */
	/* C++ 端使用继承表达类型关系，C 端使用相同布局的嵌套结构。 */
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

/* 派生快照以基类快照为前缀，调用方可根据实际类型读取对应字段。 */
#define DLMOD_ENTITY_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModEntitySnapshotV1))

#define DLMOD_PLAYER_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModPlayerSnapshotV1))
#define DLMOD_HUMAN_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModHumanSnapshotV1))
#define DLMOD_HUNTER_SNAPSHOT_V1_BASE_SIZE ((uint32_t)sizeof(DlModHunterSnapshotV1))

	/* 本地实体查询、实体读取与写入应在游戏线程调用；Hunter 运动快照可跨线程读取。 */
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
#pragma endregion

/* 模组生命周期概览：定义查询、加载、卸载、渲染和窗口消息回调。 */
#pragma region Mod Lifecycle API
	typedef DlModResult(MODSDK_CALL *DlModOnLoadFn)(
		void *modContext,
		const DlModHostApiV1 *hostApi);
	typedef void(MODSDK_CALL *DlModOnUnloadFn)(void *modContext);
	typedef void(MODSDK_CALL *DlModOnRenderFrameFn)(
		void *modContext,
		void *graphicsContext);
	typedef uint8_t(MODSDK_CALL *DlModIsUiOpenFn)(
		void *modContext);
	typedef void(MODSDK_CALL *DlModOnWindowMessageFn)(
		void *modContext,
		uintptr_t window,
		uint32_t message,
		uintptr_t wParam,
		intptr_t lParam);

	/* String fields are copied during registration; context remains owned by the mod until unload. */
	typedef struct DlModDescriptorV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		const char *id;
		const char *name;
		const char *version;
		void *context;
		DlModOnLoadFn onLoad;
		DlModOnUnloadFn onUnload;
		/* Optional directory name under <game root>/Logs. */
		const char *logName;
		/* Optional callback invoked from the host graphics presentation thread. */
		DlModOnRenderFrameFn onRenderFrame;
		/* Optional UI state callback used by the engine input router. */
		DlModIsUiOpenFn isUiOpen;
		/* Optional Win32 message callback used to feed the UI backend. */
		DlModOnWindowMessageFn onWindowMessage;
	} DlModDescriptorV1;

#define DLMOD_DESCRIPTOR_V1_MIN_SIZE ((uint32_t)offsetof(DlModDescriptorV1, logName))

	/*
	 * 所有模组必须在 ModEngineRegisterMod 调用之前注册。
	 * 如果在注册之后调用 ModEngineRegisterMod，将返回 DLMOD_RESULT_INVALID_STATE。
	 */
	typedef DlModResult(MODSDK_CALL *DlModQueryModFn)(
		uint32_t requestedAbiVersion,
		DlModDescriptorV1 *descriptor);
#pragma endregion

/* 引擎 API 概览：定义引擎初始化、模组管理及宿主转发函数表。 */
#pragma region Engine API
	typedef DlModResult(MODSDK_CALL *ModEngineInitializeFn)(
		const DlModHostApiV1 *hostApi);
	typedef DlModResult(MODSDK_CALL *ModEngineRegisterModFn)(
		const DlModDescriptorV1 *mod);
	typedef DlModResult(MODSDK_CALL *ModEngineUnregisterModFn)(
		const char *modId);
	typedef DlModResult(MODSDK_CALL *ModEngineShutdownFn)(void);
	typedef void(MODSDK_CALL *ModEngineRenderFrameFn)(
		void *graphicsContext);
	typedef uint8_t(MODSDK_CALL *ModEngineIsUiOpenFn)(void);
	typedef void(MODSDK_CALL *ModEngineOnWindowMessageFn)(
		uintptr_t window,
		uint32_t message,
		uintptr_t wParam,
		intptr_t lParam);

	typedef struct ModEngineApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		ModEngineInitializeFn initialize;
		ModEngineRegisterModFn registerMod;
		ModEngineUnregisterModFn unregisterMod;
		ModEngineShutdownFn shutdown;
		ModEngineRenderFrameFn renderFrame;
		ModEngineIsUiOpenFn isUiOpen;
		ModEngineOnWindowMessageFn onWindowMessage;
	} ModEngineApiV1;

	typedef DlModResult(MODSDK_CALL *ModSDKQueryApiFn)(
		uint32_t requestedAbiVersion,
		ModEngineApiV1 *api);

	MODSDK_API DlModResult MODSDK_CALL ModSDK_QueryApi(
		uint32_t requestedAbiVersion,
		ModEngineApiV1 *api);
#pragma endregion

#ifdef __cplusplus
}
#endif
