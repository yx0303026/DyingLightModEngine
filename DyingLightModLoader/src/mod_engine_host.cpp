#include "loader_runtime.h"

#include <array>
#include <string>

namespace {

void DLMOD_CALL HostLog(void*, uint32_t, const char* message)
{
    if (message != nullptr) {
        OutputDebugStringA("[DyingLightModLoader] ");
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }
}

HMODULE LoadModEngine(const LoaderRuntime& runtime)
{
    if (runtime.moduleHandle == nullptr) {
        return nullptr;
    }

    std::array<wchar_t, 32768> modulePath{};
    const DWORD length = GetModuleFileNameW(runtime.moduleHandle, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (length == 0 || length >= modulePath.size()) {
        return nullptr;
    }

    std::wstring enginePath(modulePath.data(), length);
    const size_t separator = enginePath.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return nullptr;
    }

    enginePath.resize(separator + 1);
    enginePath += L"DyingLightModEngine.dll";
    return LoadLibraryW(enginePath.c_str());
}

}

void InitializeModEngine(LoaderRuntime& runtime)
{
    runtime.modEngine = LoadModEngine(runtime);
    if (runtime.modEngine == nullptr) {
        OutputDebugStringA("[DyingLightModLoader] DyingLightModEngine.dll was not found beside DyingLightModLoader.dll\n");
        return;
    }

    const auto queryApi = reinterpret_cast<DlModEngineQueryApiFn>(GetProcAddress(runtime.modEngine, "DlModEngine_QueryApi"));
    if (queryApi == nullptr) {
        OutputDebugStringA("[DyingLightModLoader] Engine ABI entry point was not found\n");
        FreeLibrary(runtime.modEngine);
        runtime.modEngine = nullptr;
        return;
    }

    runtime.engineApi.structSize = sizeof(runtime.engineApi);
    if (queryApi(DLMOD_ENGINE_ABI_VERSION, &runtime.engineApi) != DLMOD_RESULT_SUCCESS ||
        runtime.engineApi.structSize < sizeof(DlModEngineApiV1) ||
        runtime.engineApi.abiVersion != DLMOD_ENGINE_ABI_VERSION ||
        runtime.engineApi.initialize == nullptr || runtime.engineApi.registerMod == nullptr ||
        runtime.engineApi.unregisterMod == nullptr || runtime.engineApi.shutdown == nullptr) {
        OutputDebugStringA("[DyingLightModLoader] Engine ABI negotiation failed\n");
        FreeLibrary(runtime.modEngine);
        runtime.modEngine = nullptr;
        runtime.engineApi = {};
        return;
    }

    const DlModHostApiV1 hostApi = {
        sizeof(DlModHostApiV1),
        DLMOD_ENGINE_ABI_VERSION,
        nullptr,
        &HostLog
    };
    if (runtime.engineApi.initialize(&hostApi) != DLMOD_RESULT_SUCCESS) {
        OutputDebugStringA("[DyingLightModLoader] Engine initialization failed\n");
        FreeLibrary(runtime.modEngine);
        runtime.modEngine = nullptr;
        runtime.engineApi = {};
    }
}
