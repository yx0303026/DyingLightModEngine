#include "modsdk_abi.h"
#include "modsdk_logger.h"

namespace {

struct ExampleModState {
    void* hostContext = nullptr;
    DlModLogFn hostLog = nullptr;
};

ExampleModState g_exampleModState;

DlModResult MODSDK_CALL OnLoad(void* modContext, const DlModHostApiV1* hostApi)
{
    if (modContext != &g_exampleModState ||
        hostApi == nullptr ||
        hostApi->structSize < sizeof(DlModHostApiV1) ||
        hostApi->abiVersion != MODSDK_ABI_VERSION ||
        hostApi->log == nullptr) {
        return DLMOD_RESULT_INVALID_ARGUMENT;
    }

    g_exampleModState.hostContext = hostApi->context;
    g_exampleModState.hostLog = hostApi->log;
    g_exampleModState.hostLog(
        g_exampleModState.hostContext,
        static_cast<uint32_t>(ModSDKLogLevel::Info),
        "Example Mod loaded successfully.");
    return DLMOD_RESULT_SUCCESS;
}

void MODSDK_CALL OnUnload(void* modContext)
{
    if (modContext != &g_exampleModState) {
        return;
    }

    if (g_exampleModState.hostLog != nullptr) {
        g_exampleModState.hostLog(
            g_exampleModState.hostContext,
            static_cast<uint32_t>(ModSDKLogLevel::Info),
            "Example Mod unloaded.");
    }
    g_exampleModState = {};
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
        "example.mod",
        "Example Mod",
        "1.0.0",
        &g_exampleModState,
        &OnLoad,
        &OnUnload
    };
    return DLMOD_RESULT_SUCCESS;
}
