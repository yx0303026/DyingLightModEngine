#pragma once

#include <stdint.h>
#include <stddef.h>

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

#ifdef __cplusplus
extern "C" {
#endif

#define MODSDK_ABI_VERSION 1u

	typedef enum DlModResult {
		DLMOD_RESULT_SUCCESS = 0,
		DLMOD_RESULT_INVALID_ARGUMENT = 1,
		DLMOD_RESULT_UNSUPPORTED_ABI = 2,
		DLMOD_RESULT_INVALID_STATE = 3,
		DLMOD_RESULT_NOT_FOUND = 4,
		DLMOD_RESULT_IO_ERROR = 5,
		DLMOD_RESULT_PARTIAL_SUCCESS = 6
	} DlModResult;

	typedef void(MODSDK_CALL* EngineLogCallback)(void* context, uint32_t level, const char* message);

#define DLMOD_UI_MOUSE_POSITION_VALID 0x1u
#define DLMOD_UI_MOUSE_DEVICE_CAPTURED 0x2u

	/*
	 * Mouse state sampled from the game's DirectInput mouse device while a mod UI
	 * is open. Games that read the mouse through DirectInput keep receiving the
	 * device state even when window messages are filtered, so the engine neutralizes
	 * that device and forwards its relative motion to the UI instead.
	 */
	typedef struct DlModUiMouseStateV1 {
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
	typedef uint8_t(MODSDK_CALL* ModEngineGetUiMouseStateFn)(
		DlModUiMouseStateV1* state);

	typedef struct DlModHostApiV1 {
		uint32_t structSize;
		uint32_t abiVersion;
		void* context;
		EngineLogCallback log;
		/* Optional DirectInput mouse bridge; null when the engine predates it. */
		ModEngineGetUiMouseStateFn getUiMouseState;
	} DlModHostApiV1;

#define DLMOD_HOST_API_V1_MOUSE_STATE_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, getUiMouseState) + sizeof(ModEngineGetUiMouseStateFn)))

	typedef DlModResult(MODSDK_CALL* DlModOnLoadFn)(
		void* modContext,
		const DlModHostApiV1* hostApi);
	typedef void(MODSDK_CALL* DlModOnUnloadFn)(void* modContext);
	typedef void(MODSDK_CALL* DlModOnRenderFrameFn)(
		void* modContext,
		void* graphicsContext);
	typedef uint8_t(MODSDK_CALL* DlModIsUiOpenFn)(
		void* modContext);
	typedef void(MODSDK_CALL* DlModOnWindowMessageFn)(
		void* modContext,
		uintptr_t window,
		uint32_t message,
		uintptr_t wParam,
		intptr_t lParam);

	/* String fields are copied during registration; context remains owned by the mod until unload. */
	typedef struct DlModDescriptorV1 {
		uint32_t structSize;
		uint32_t abiVersion;
		const char* id;
		const char* name;
		const char* version;
		void* context;
		DlModOnLoadFn onLoad;
		DlModOnUnloadFn onUnload;
		/* Optional directory name under <game root>/Logs. */
		const char* logName;
		/* Optional callback invoked from the host graphics presentation thread. */
		DlModOnRenderFrameFn onRenderFrame;
		/* Optional UI state callback used by the engine input router. */
		DlModIsUiOpenFn isUiOpen;
		/* Optional Win32 message callback used to feed the UI backend. */
		DlModOnWindowMessageFn onWindowMessage;
	} DlModDescriptorV1;

#define DLMOD_DESCRIPTOR_V1_MIN_SIZE ((uint32_t)offsetof(DlModDescriptorV1, logName))

	/*
	 * Every loadable mod DLL must export a function named ModSDK_QueryMod.
	 * The manager calls it after loading the DLL to obtain the mod descriptor.
	 */
	typedef DlModResult(MODSDK_CALL* DlModQueryModFn)(
		uint32_t requestedAbiVersion,
		DlModDescriptorV1* descriptor);

	typedef DlModResult(MODSDK_CALL* ModEngineInitializeFn)(
		const DlModHostApiV1* hostApi);
	typedef DlModResult(MODSDK_CALL* ModEngineRegisterModFn)(
		const DlModDescriptorV1* mod);
	typedef DlModResult(MODSDK_CALL* ModEngineUnregisterModFn)(
		const char* modId);
	typedef DlModResult(MODSDK_CALL* ModEngineShutdownFn)(void);
	typedef void(MODSDK_CALL* ModEngineRenderFrameFn)(
		void* graphicsContext);
	typedef uint8_t(MODSDK_CALL* ModEngineIsUiOpenFn)(void);
	typedef void(MODSDK_CALL* ModEngineOnWindowMessageFn)(
		uintptr_t window,
		uint32_t message,
		uintptr_t wParam,
		intptr_t lParam);

	typedef struct ModEngineApiV1 {
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

	typedef DlModResult(MODSDK_CALL* ModSDKQueryApiFn)(
		uint32_t requestedAbiVersion,
		ModEngineApiV1* api);

	MODSDK_API DlModResult MODSDK_CALL ModSDK_QueryApi(
		uint32_t requestedAbiVersion,
		ModEngineApiV1* api);

#ifdef __cplusplus
}
#endif
