#pragma once

#include "sdk/EngineAbiTypes.h"

/* Hook ABI 概览：定义共享 detour 注册函数及其版本化函数表。 */
#ifdef __cplusplus
extern "C"
{
#endif

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

#ifdef __cplusplus
}
#endif
