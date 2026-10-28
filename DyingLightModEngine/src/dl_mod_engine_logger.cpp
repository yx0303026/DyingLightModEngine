#include "runtime/modsdk_runtime.h"

#include <cstdarg>
#include <cstdio>

void Logger::Log(ModSDKLogLevel level, const char* message) noexcept
{
	if (message == nullptr) {
		return;
	}

	DlModLogFn hostLogger = nullptr;
	void* hostContext = nullptr;
	if (ModSDK_GetHostLogger(&hostLogger, &hostContext)) {
		try {
			hostLogger(hostContext, static_cast<uint32_t>(level), message);
		} catch (...) {
		}
	}
}

void Logger::Logf(ModSDKLogLevel level, const char* format, ...) noexcept
{
	if (format == nullptr) {
		return;
	}

	char message[2048]{};
	va_list arguments;
	va_start(arguments, format);
	_vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
	va_end(arguments);

	Log(level, message);
}

void Logger::Trace(const char* message) noexcept
{
	Log(ModSDKLogLevel::Trace, message);
}

void Logger::Debug(const char* message) noexcept
{
	Log(ModSDKLogLevel::Debug, message);
}

void Logger::Info(const char* message) noexcept
{
	Log(ModSDKLogLevel::Info, message);
}

void Logger::Warning(const char* message) noexcept
{
	Log(ModSDKLogLevel::Warning, message);
}

void Logger::Error(const char* message) noexcept
{
	Log(ModSDKLogLevel::Error, message);
}

void Logger::Critical(const char* message) noexcept
{
	Log(ModSDKLogLevel::Critical, message);
}
