#include "loader_runtime.h"

namespace {

INIT_ONCE g_initializeOnce = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK InitializeLoader(PINIT_ONCE, PVOID, PVOID*)
{
    InitializeModEngine(GetLoaderRuntime());
    return TRUE;
}

}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        SetLoaderModule(instance);
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

extern "C" HRESULT WINAPI DyingLightModLoader_Initialize()
{
    return InitOnceExecuteOnce(&g_initializeOnce, &InitializeLoader, nullptr, nullptr) ? S_OK : E_FAIL;
}
