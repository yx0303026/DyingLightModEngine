#include "dl_mod_engine_host.h"

#include "modsdk_abi.h"
#include "modsdk_logger.h"
#include "modsdk_mod_manager.h"

bool InitializeModEngine(HMODULE engineModule) noexcept
{
    InitializeEngineLogger(engineModule);

    DlModEngineApiV1 engineApi{};
    engineApi.structSize = sizeof(engineApi);
    if (ModSDK_QueryApi(MODSDK_ABI_VERSION, &engineApi) != DLMOD_RESULT_SUCCESS ||
        engineApi.structSize < sizeof(DlModEngineApiV1) ||
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
        &EngineHostLog
    };
    if (engineApi.initialize(&hostApi) != DLMOD_RESULT_SUCCESS) {
        OutputDebugStringA("[DyingLightModEngine] Engine initialization failed\n");
        return false;
    }

    const DlModResult loadResult = ModManager::LoadAll();
    if (loadResult == DLMOD_RESULT_PARTIAL_SUCCESS || loadResult == DLMOD_RESULT_IO_ERROR) {
        OutputDebugStringA("[DyingLightModEngine] One or more mods could not be loaded\n");
    } else if (loadResult != DLMOD_RESULT_SUCCESS) {
        OutputDebugStringA("[DyingLightModEngine] ModManager initialization failed\n");
        engineApi.shutdown();
        return false;
    }
    return true;
}

bool ShutdownModEngine() noexcept
{
    DlModEngineApiV1 engineApi{};
    engineApi.structSize = sizeof(engineApi);
    if (ModSDK_QueryApi(MODSDK_ABI_VERSION, &engineApi) != DLMOD_RESULT_SUCCESS ||
        engineApi.structSize < sizeof(DlModEngineApiV1) ||
        engineApi.abiVersion != MODSDK_ABI_VERSION ||
        engineApi.shutdown == nullptr) {
        return false;
    }

    const DlModResult unloadResult = ModManager::UnloadAll();
    const DlModResult shutdownResult = engineApi.shutdown();
    return unloadResult == DLMOD_RESULT_SUCCESS && shutdownResult == DLMOD_RESULT_SUCCESS;
}
