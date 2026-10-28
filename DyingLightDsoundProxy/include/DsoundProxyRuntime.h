#pragma once

#include <Windows.h>

struct DsoundProxyRuntime {
    INIT_ONCE initOnce = INIT_ONCE_STATIC_INIT;
    HMODULE moduleHandle = nullptr;
    HMODULE systemDsound = nullptr;
    HMODULE modEngine = nullptr;
};

void SetDsoundProxyModule(HMODULE module);
FARPROC ResolveDsoundExport(const char* name);
