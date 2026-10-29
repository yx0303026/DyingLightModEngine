#pragma once

#include <Windows.h>

bool InitializeModEngine(HMODULE engineModule) noexcept;
bool ShutdownModEngine() noexcept;
