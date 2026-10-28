#include "loader_runtime.h"

namespace {

void MODSDK_CALL HostLog(void*, uint32_t, const char* message)
{
    if (message != nullptr) {
        OutputDebugStringA("[DyingLightModLoader] ");
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }
}

}

void InitializeModEngine(LoaderRuntime& runtime)
{
    runtime.engineApi.structSize = sizeof(runtime.engineApi);
    if (ModSDK_QueryApi(MODSDK_ABI_VERSION, &runtime.engineApi) != DLMOD_RESULT_SUCCESS ||
        runtime.engineApi.structSize < sizeof(DlModEngineApiV1) ||
        runtime.engineApi.abiVersion != MODSDK_ABI_VERSION ||
        runtime.engineApi.initialize == nullptr || runtime.engineApi.registerMod == nullptr ||
        runtime.engineApi.unregisterMod == nullptr || runtime.engineApi.shutdown == nullptr) {
        OutputDebugStringA("[DyingLightModLoader] Engine ABI negotiation failed\n");
        runtime.engineApi = {};
        return;
    }

    const DlModHostApiV1 hostApi = {
        sizeof(DlModHostApiV1),
        MODSDK_ABI_VERSION,
        nullptr,
        &HostLog
    };
    if (runtime.engineApi.initialize(&hostApi) != DLMOD_RESULT_SUCCESS) {
        OutputDebugStringA("[DyingLightModLoader] Engine initialization failed\n");
        runtime.engineApi = {};
    }
}
