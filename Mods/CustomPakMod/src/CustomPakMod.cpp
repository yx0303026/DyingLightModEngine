#include "sdk/EngineAbi.h"
#include "PakLoader.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {

struct CustomPakModState {
	void* hostContext = nullptr;
	EngineLogCallback hostLog = nullptr;
};

CustomPakModState g_customPakModState;

DlModResult MODSDK_CALL OnLoad(void* modContext, const DlModHostApiV1* hostApi)
{
	if (modContext != &g_customPakModState ||
		hostApi == nullptr ||
		hostApi->structSize < sizeof(DlModHostApiV1) ||
		hostApi->abiVersion != MODSDK_ABI_VERSION ||
		hostApi->log == nullptr) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}

	g_customPakModState.hostContext = hostApi->context;
	g_customPakModState.hostLog = hostApi->log;

	const HMODULE ownerModule = reinterpret_cast<HMODULE>(&__ImageBase);
	if (!custom_pak_mod::StartPakLoading(ownerModule, hostApi)) {
		return DLMOD_RESULT_INVALID_STATE;
	}
	return DLMOD_RESULT_SUCCESS;
}

void MODSDK_CALL OnUnload(void* modContext)
{
	if (modContext != &g_customPakModState) {
		return;
	}

	custom_pak_mod::StopPakLoading();
	g_customPakModState = {};
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
		"custom_pak.mod",
		"Custom Pak Mod",
		"1.0.0",
		&g_customPakModState,
		&OnLoad,
		&OnUnload,
		"CustomPakMod"
	};
	return DLMOD_RESULT_SUCCESS;
}
