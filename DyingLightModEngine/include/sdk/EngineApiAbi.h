#pragma once

#include "sdk/EngineModLifecycleAbi.h"
#include "sdk/EngineHostAbi.h"

/* 引擎 API 概览：定义引擎初始化、模组管理、渲染与窗口消息转发函数表。 */
#ifdef __cplusplus
extern "C"
{
#endif

	typedef DlModResult(MODSDK_CALL *ModEngineInitializeFn)(const DlModHostApiV1 *hostApi);
	typedef DlModResult(MODSDK_CALL *ModEngineRegisterModFn)(const DlModDescriptorV1 *mod);
	typedef DlModResult(MODSDK_CALL *ModEngineUnregisterModFn)(const char *modId);
	typedef DlModResult(MODSDK_CALL *ModEngineShutdownFn)(void);
	typedef void(MODSDK_CALL *ModEngineRenderFrameFn)(void *graphicsContext);
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

#ifdef __cplusplus
}
#endif
