#include "PlayerNameMaskRuntime.h"

#include "sdk/EngineModLifecycleAbi.h"

namespace
{
	// 区域概览：提供标准模组生命周期入口，不扩展任何 SDK ABI。
	player_name_mask::PlayerNameMaskRuntime g_runtime;

	DlModResult MODSDK_CALL OnLoad(
		void* modContext,
		const DlModHostApiV1* hostApi)
	{
		return modContext == &g_runtime
			? g_runtime.Initialize(hostApi)
			: DLMOD_RESULT_INVALID_ARGUMENT;
	}

	void MODSDK_CALL OnUnload(void* modContext)
	{
		if (modContext == &g_runtime)
			g_runtime.Shutdown();
	}
}

extern "C" DlModResult MODSDK_CALL ModSDK_QueryMod(
	uint32_t requestedAbiVersion,
	DlModDescriptorV1* descriptor)
{
	if (descriptor == nullptr || descriptor->structSize < sizeof(DlModDescriptorV1))
		return DLMOD_RESULT_INVALID_ARGUMENT;
	if (requestedAbiVersion != MODSDK_ABI_VERSION)
		return DLMOD_RESULT_UNSUPPORTED_ABI;

	*descriptor = {
		sizeof(DlModDescriptorV1),
		MODSDK_ABI_VERSION,
		"player_name_mask.mod",
		"Player Name Mask",
		"1.0.0",
		&g_runtime,
		&OnLoad,
		&OnUnload,
		"PlayerNameMask",
		nullptr,
		nullptr,
		nullptr
	};
	return DLMOD_RESULT_SUCCESS;
}
