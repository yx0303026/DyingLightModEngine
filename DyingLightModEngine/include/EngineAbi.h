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

	typedef struct DlModHostApiV1 {
		uint32_t structSize;
		uint32_t abiVersion;
		void* context;
		EngineLogCallback log;
	} DlModHostApiV1;

	typedef DlModResult(MODSDK_CALL* DlModOnLoadFn)(
		void* modContext,
		const DlModHostApiV1* hostApi);
	typedef void(MODSDK_CALL* DlModOnUnloadFn)(void* modContext);
	typedef void(MODSDK_CALL* DlModOnRenderFrameFn)(
		void* modContext,
		void* graphicsContext);

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

	typedef struct ModEngineApiV1 {
		uint32_t structSize;
		uint32_t abiVersion;
		ModEngineInitializeFn initialize;
		ModEngineRegisterModFn registerMod;
		ModEngineUnregisterModFn unregisterMod;
		ModEngineShutdownFn shutdown;
		ModEngineRenderFrameFn renderFrame;
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
