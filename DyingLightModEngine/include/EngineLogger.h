#pragma once

#include <stdint.h>
#include <Windows.h>

#include "EngineAbi.h"

#ifdef __cplusplus

enum class EngineLogLevel : uint32_t {
	Trace = 0,
	Debug = 1,
	Info = 2,
	Warning = 3,
	Error = 4,
	Critical = 5
};

class EngineLogger final {
public:
	static MODSDK_API bool Initialize(HMODULE engineModule) noexcept;
	static MODSDK_API void MODSDK_CALL HostCallback(void* context, uint32_t level, const char* message) noexcept;

	static MODSDK_API void Log(EngineLogLevel level, const char* message) noexcept;
	static MODSDK_API void Logf(EngineLogLevel level, const char* format, ...) noexcept;

	static MODSDK_API void Trace(const char* message) noexcept;
	static MODSDK_API void Debug(const char* message) noexcept;
	static MODSDK_API void Info(const char* message) noexcept;
	static MODSDK_API void Warning(const char* message) noexcept;
	static MODSDK_API void Error(const char* message) noexcept;
	static MODSDK_API void Critical(const char* message) noexcept;
};

#endif
