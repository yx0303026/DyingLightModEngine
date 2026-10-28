#pragma once

#include <Windows.h>

#include "ModSDK.h"

struct LoaderRuntime {
    HMODULE moduleHandle = nullptr;
    DlModEngineApiV1 engineApi{};
};

void SetLoaderModule(HMODULE module);
LoaderRuntime& GetLoaderRuntime();
void InitializeModEngine(LoaderRuntime& runtime);
