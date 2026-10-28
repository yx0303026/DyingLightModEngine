#include "EngineHost.h"

#include "EngineAbi.h"
#include "EngineLogger.h"
#include "EngineModManager.h"

bool InitializeModEngine(HMODULE engineModule) noexcept
{
	EngineLogger::Initialize(engineModule);

	ModEngineApiV1 engineApi{};
	engineApi.structSize = sizeof(engineApi);
	if (ModSDK_QueryApi(MODSDK_ABI_VERSION, &engineApi) != DLMOD_RESULT_SUCCESS ||
		engineApi.structSize < sizeof(ModEngineApiV1) ||
		engineApi.abiVersion != MODSDK_ABI_VERSION ||
		engineApi.initialize == nullptr || engineApi.registerMod == nullptr ||
		engineApi.unregisterMod == nullptr || engineApi.shutdown == nullptr) {
		OutputDebugStringA("[DyingLightModEngine] Engine ABI negotiation failed\n");
		return false;
	}

	const DlModHostApiV1 hostApi = {
		sizeof(DlModHostApiV1),
		MODSDK_ABI_VERSION,
		nullptr,
		&EngineLogger::HostCallback
	};
	if (engineApi.initialize(&hostApi) != DLMOD_RESULT_SUCCESS) {
		OutputDebugStringA("[DyingLightModEngine] Engine initialization failed\n");
		return false;
	}

	const DlModResult loadResult = EngineModManager::LoadAll();
	if (loadResult == DLMOD_RESULT_PARTIAL_SUCCESS || loadResult == DLMOD_RESULT_IO_ERROR) {
		EngineLogger::Warning("One or more mods could not be loaded.");
	}
	else if (loadResult != DLMOD_RESULT_SUCCESS) {
		EngineLogger::Error("ModManager initialization failed.");
		engineApi.shutdown();
		return false;
	}
	return true;
}

bool ShutdownModEngine() noexcept
{
	ModEngineApiV1 engineApi{};
	engineApi.structSize = sizeof(engineApi);
	if (ModSDK_QueryApi(MODSDK_ABI_VERSION, &engineApi) != DLMOD_RESULT_SUCCESS ||
		engineApi.structSize < sizeof(ModEngineApiV1) ||
		engineApi.abiVersion != MODSDK_ABI_VERSION ||
		engineApi.shutdown == nullptr) {
		return false;
	}

	const DlModResult unloadResult = EngineModManager::UnloadAll();
	if (unloadResult != DLMOD_RESULT_SUCCESS) {
		EngineLogger::Error("Could not unload all mods; engine shutdown was cancelled.");
		return false;
	}

	const DlModResult shutdownResult = engineApi.shutdown();
	return shutdownResult == DLMOD_RESULT_SUCCESS;
}
