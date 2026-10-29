#include "core/EngineHost.h"

#include "sdk/EngineApiAbi.h"
#include "runtime/EngineDirectInputRuntime.h"
#include "runtime/EngineCollisionRuntime.h"
#include "runtime/EngineEntityRuntime.h"
#include "runtime/EngineLobbyRuntime.h"
#include "runtime/EngineGraphicsRuntime.h"
#include "runtime/EngineHookRuntime.h"
#include "runtime/EngineCameraRuntime.h"
#include "runtime/EngineProjectileManager.h"
#include "logging/EngineLogger.h"
#include "managers/EngineModManager.h"

namespace {

EngineGraphicsRuntime g_graphicsRuntime;

}

bool InitializeModEngine(HMODULE engineModule) noexcept
{
	if (!EngineLogger::Initialize(engineModule)) {
		OutputDebugStringA("[DyingLightModEngine] Logger initialization failed\n");
		return false;
	}

	ModEngineApiV1 engineApi{};
	engineApi.structSize = sizeof(engineApi);
	if (ModSDK_QueryApi(MODSDK_ABI_VERSION, &engineApi) != DLMOD_RESULT_SUCCESS ||
		engineApi.structSize < sizeof(ModEngineApiV1) ||
		engineApi.abiVersion != MODSDK_ABI_VERSION ||
		engineApi.initialize == nullptr || engineApi.registerMod == nullptr ||
		engineApi.unregisterMod == nullptr || engineApi.shutdown == nullptr ||
		engineApi.renderFrame == nullptr || engineApi.isUiOpen == nullptr ||
		engineApi.onWindowMessage == nullptr) {
		OutputDebugStringA("[DyingLightModEngine] Engine ABI negotiation failed\n");
		return false;
	}

	const DlModHostApiV1 hostApi = {
		sizeof(DlModHostApiV1),
		MODSDK_ABI_VERSION,
		nullptr,
		&EngineLogger::HostCallback,
		&EngineDirectInputRuntime::GetUiMouseState,
		EngineCollisionRuntime::GetApi(),
		EngineEntityRuntime::GetApi(),
		EngineHookRuntime::GetApi(),
		EngineLobbyRuntime::GetApi(),
		EngineProjectileManager::GetApi(),
		EngineCameraRuntime::GetApi()
	};
	if (engineApi.initialize(&hostApi) != DLMOD_RESULT_SUCCESS) {
		OutputDebugStringA("[DyingLightModEngine] Engine initialization failed\n");
		return false;
	}
	if (!EngineLobbyRuntime::Initialize()) {
		EngineLogger::Warning("Lobby collector initialization failed; the Lobby API remains unavailable.");
	}

	if (!EngineDirectInputRuntime::Install()) {
		EngineLogger::Warning("DirectInput mouse hook initialization failed; mod UI input falls back to window messages.");
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
	if (!g_graphicsRuntime.Initialize()) {
		EngineLogger::Warning("DX11 graphics hook initialization failed; ImGui modules will remain unavailable.");
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
		engineApi.shutdown == nullptr || engineApi.isUiOpen == nullptr ||
		engineApi.onWindowMessage == nullptr) {
		return false;
	}

	g_graphicsRuntime.Shutdown();
	EngineDirectInputRuntime::Uninstall();
	EngineLobbyRuntime::Shutdown();
	const DlModResult unloadResult = EngineModManager::UnloadAll();
	if (unloadResult != DLMOD_RESULT_SUCCESS) {
		EngineLogger::Error("Could not unload all mods; engine shutdown was cancelled.");
		return false;
	}

	const DlModResult shutdownResult = engineApi.shutdown();
	if (shutdownResult == DLMOD_RESULT_SUCCESS) {
		EngineProjectileManager::Shutdown();
		EngineEntityRuntime::Shutdown();
	}
	return shutdownResult == DLMOD_RESULT_SUCCESS;
}
