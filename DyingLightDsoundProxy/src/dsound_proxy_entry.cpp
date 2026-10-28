#include "dsound_proxy_runtime.h"

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        SetDsoundProxyModule(instance);
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
