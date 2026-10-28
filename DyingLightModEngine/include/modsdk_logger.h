#pragma once

#include <stdint.h>

#include "modsdk_abi.h"

#ifdef __cplusplus

#if defined(_WIN32)
#include <Windows.h>
#endif

enum class ModSDKLogLevel : uint32_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5
};

class Logger final {
public:
    static MODSDK_API void Log(ModSDKLogLevel level, const char* message) noexcept;
    static MODSDK_API void Logf(ModSDKLogLevel level, const char* format, ...) noexcept;

    static MODSDK_API void Trace(const char* message) noexcept;
    static MODSDK_API void Debug(const char* message) noexcept;
    static MODSDK_API void Info(const char* message) noexcept;
    static MODSDK_API void Warning(const char* message) noexcept;
    static MODSDK_API void Error(const char* message) noexcept;
    static MODSDK_API void Critical(const char* message) noexcept;
};

#if defined(_WIN32)
bool ModSDK_GetHostLogger(DlModLogFn* logger, void** context) noexcept;
bool InitializeEngineLogger(HMODULE engineModule) noexcept;
void MODSDK_CALL EngineHostLog(void* context, uint32_t level, const char* message) noexcept;
#endif

#endif
