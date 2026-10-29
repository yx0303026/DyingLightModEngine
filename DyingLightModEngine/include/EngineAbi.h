#pragma once

#include <stdint.h>
#include <stddef.h>

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
	} DlModHostApiV1;

#define DLMOD_HOST_API_V1_MOUSE_STATE_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, getUiMouseState) + sizeof(ModEngineGetUiMouseStateFn)))
#define DLMOD_HOST_API_V1_COLLISION_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, collision) + sizeof(const DlModCollisionApiV1 *)))
#pragma endregion

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
