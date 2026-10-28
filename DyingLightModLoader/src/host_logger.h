#pragma once

#include <Windows.h>

#include "ModSDK.h"

bool InitializeHostLogger(HMODULE loaderModule) noexcept;
void MODSDK_CALL HostLog(void* context, uint32_t level, const char* message) noexcept;
