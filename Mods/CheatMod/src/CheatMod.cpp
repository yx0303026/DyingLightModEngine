#include "CheatModRuntime.h"

namespace {

	cheat_mod::CheatModRuntime g_runtime;

	DlModResult MODSDK_CALL OnLoad(void* modContext, const DlModHostApiV1* hostApi)
	{
		return modContext == &g_runtime ? g_runtime.Initialize(hostApi) : DLMOD_RESULT_INVALID_ARGUMENT;
	}

	void MODSDK_CALL OnUnload(void* modContext)
	{
		if (modContext == &g_runtime) {
			g_runtime.Shutdown();
		}
	}

	void MODSDK_CALL OnRenderFrame(void* modContext, void* graphicsContext)
	{
		if (modContext == &g_runtime) {
			g_runtime.RenderFrame(graphicsContext);
		}
	}

}

extern "C" DlModResult MODSDK_CALL ModSDK_QueryMod(
	uint32_t requestedAbiVersion,
	DlModDescriptorV1* descriptor)
{
	if (descriptor == nullptr || descriptor->structSize < sizeof(DlModDescriptorV1)) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (requestedAbiVersion != MODSDK_ABI_VERSION) {
		return DLMOD_RESULT_UNSUPPORTED_ABI;
	}

	*descriptor = {
		sizeof(DlModDescriptorV1),
		MODSDK_ABI_VERSION,
		"cheat.mod",
		"Cheat Mod",
		"1.0.0",
		&g_runtime,
		&OnLoad,
		&OnUnload,
		"CheatMod",
		&OnRenderFrame
	};
	return DLMOD_RESULT_SUCCESS;
}

extern "C" MODSDK_MOD_EXPORT void MODSDK_CALL CheatMod_RenderFrame()
{
	g_runtime.RenderFrame(nullptr);
}

extern "C" MODSDK_MOD_EXPORT void MODSDK_CALL CheatMod_HandleKeyEvent(
	uint32_t virtualKey, uint8_t isDown)
{
	g_runtime.HandleKeyEvent(virtualKey, isDown != 0U);
}

extern "C" MODSDK_MOD_EXPORT uint8_t MODSDK_CALL CheatMod_IsOpen()
{
	return g_runtime.IsOpen() ? 1U : 0U;
}
