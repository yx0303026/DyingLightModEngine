#include "dsound_proxy_runtime.h"

#include <array>
#include <string>

namespace {

DsoundProxyRuntime g_runtime;

std::wstring GetModuleDirectory(HMODULE module)
{
    std::array<wchar_t, 32768> modulePath{};
    const DWORD length = GetModuleFileNameW(module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (length == 0 || length >= modulePath.size()) {
        return {};
    }

    std::wstring directory(modulePath.data(), length);
    const size_t separator = directory.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return {};
    }
    directory.resize(separator + 1);
    return directory;
}

HMODULE LoadSystemDsound()
{
    std::array<wchar_t, MAX_PATH> systemDirectory{};
    const UINT length = GetSystemDirectoryW(systemDirectory.data(), static_cast<UINT>(systemDirectory.size()));
    if (length == 0 || length >= systemDirectory.size()) {
        return nullptr;
    }

    std::wstring systemPath(systemDirectory.data(), length);
    systemPath += L"\\dsound.dll";
    return LoadLibraryW(systemPath.c_str());
}

HMODULE LoadModLoader()
{
    const std::wstring moduleDirectory = GetModuleDirectory(g_runtime.moduleHandle);
    if (moduleDirectory.empty()) {
        return nullptr;
    }
    return LoadLibraryW((moduleDirectory + L"DyingLightModLoader.dll").c_str());
}

BOOL CALLBACK InitializeProxy(PINIT_ONCE, PVOID, PVOID*)
{
    g_runtime.systemDsound = LoadSystemDsound();
    if (g_runtime.systemDsound == nullptr) {
        OutputDebugStringA("[DyingLightDsoundProxy] Could not load the system dsound.dll\n");
        return TRUE;
    }

    g_runtime.modLoader = LoadModLoader();
    if (g_runtime.modLoader == nullptr) {
        OutputDebugStringA("[DyingLightDsoundProxy] DyingLightModLoader.dll was not found beside dsound.dll\n");
        return TRUE;
    }

    using InitializeLoaderFn = HRESULT(WINAPI*)(void);
    const auto initializeLoader = reinterpret_cast<InitializeLoaderFn>(
        GetProcAddress(g_runtime.modLoader, "DyingLightModLoader_Initialize"));
    if (initializeLoader == nullptr || FAILED(initializeLoader())) {
        OutputDebugStringA("[DyingLightDsoundProxy] DyingLightModLoader initialization failed\n");
        FreeLibrary(g_runtime.modLoader);
        g_runtime.modLoader = nullptr;
    }
    return TRUE;
}

}

void SetDsoundProxyModule(HMODULE module)
{
    g_runtime.moduleHandle = module;
}

FARPROC ResolveDsoundExport(const char* name)
{
    if (!InitOnceExecuteOnce(&g_runtime.initOnce, &InitializeProxy, nullptr, nullptr) || g_runtime.systemDsound == nullptr) {
        return nullptr;
    }
    return GetProcAddress(g_runtime.systemDsound, name);
}
