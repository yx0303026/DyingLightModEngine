#include "EngineLogger.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace {

	std::mutex g_logMutex;
	HANDLE g_logFile = INVALID_HANDLE_VALUE;

	const char* GetLogLevelName(EngineLogLevel level) noexcept
	{
		switch (level) {
		case EngineLogLevel::Trace:
			return "TRACE";
		case EngineLogLevel::Debug:
			return "DEBUG";
		case EngineLogLevel::Info:
			return "INFO";
		case EngineLogLevel::Warning:
			return "WARN";
		case EngineLogLevel::Error:
			return "ERROR";
		case EngineLogLevel::Critical:
			return "CRITICAL";
		default:
			return "UNKNOWN";
		}
	}

	std::wstring GetLogFilePath(HMODULE engineModule)
	{
		std::array<wchar_t, 32768> modulePath{};
		const DWORD length = GetModuleFileNameW(
			engineModule,
			modulePath.data(),
			static_cast<DWORD>(modulePath.size()));
		if (length == 0 || length >= modulePath.size()) {
			return {};
		}

		std::wstring path(modulePath.data(), length);
		const size_t separator = path.find_last_of(L"\\/");
		if (separator == std::wstring::npos) {
			return {};
		}
		path.resize(separator + 1);
		path += L"DyingLightModEngine.log";
		return path;
	}

	void WriteLogLine(EngineLogLevel level, const char* message) noexcept
	{
		SYSTEMTIME timestamp{};
		GetLocalTime(&timestamp);

		std::array<char, 256> prefix{};
		const int prefixLength = std::snprintf(
			prefix.data(),
			prefix.size(),
			"[%04u-%02u-%02u %02u:%02u:%02u.%03u] [%s] ",
			static_cast<unsigned int>(timestamp.wYear),
			static_cast<unsigned int>(timestamp.wMonth),
			static_cast<unsigned int>(timestamp.wDay),
			static_cast<unsigned int>(timestamp.wHour),
			static_cast<unsigned int>(timestamp.wMinute),
			static_cast<unsigned int>(timestamp.wSecond),
			static_cast<unsigned int>(timestamp.wMilliseconds),
			GetLogLevelName(level));
		if (prefixLength <= 0 || static_cast<size_t>(prefixLength) >= prefix.size()) {
			return;
		}

		OutputDebugStringA(prefix.data());
		OutputDebugStringA(message);
		OutputDebugStringA("\n");

		try {
			const std::lock_guard<std::mutex> lock(g_logMutex);
			if (g_logFile == INVALID_HANDLE_VALUE) {
				return;
			}

			DWORD bytesWritten = 0;
			WriteFile(
				g_logFile,
				prefix.data(),
				static_cast<DWORD>(prefixLength),
				&bytesWritten,
				nullptr);
			WriteFile(
				g_logFile,
				message,
				static_cast<DWORD>(std::strlen(message)),
				&bytesWritten,
				nullptr);
			WriteFile(g_logFile, "\r\n", 2, &bytesWritten, nullptr);
		}
		catch (...) {
		}
	}

}

bool EngineLogger::Initialize(HMODULE engineModule) noexcept
{
	if (engineModule == nullptr) {
		return false;
	}

	try {
		const std::wstring path = GetLogFilePath(engineModule);
		if (path.empty()) {
			return false;
		}

		const std::lock_guard<std::mutex> lock(g_logMutex);
		if (g_logFile != INVALID_HANDLE_VALUE) {
			return true;
		}

		g_logFile = CreateFileW(
			path.c_str(),
			FILE_APPEND_DATA,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);
		return g_logFile != INVALID_HANDLE_VALUE;
	}
	catch (...) {
		return false;
	}
}

void MODSDK_CALL EngineLogger::HostCallback(
	void*,
	uint32_t level,
	const char* message) noexcept
{
	if (message == nullptr || level > static_cast<uint32_t>(EngineLogLevel::Critical)) {
		return;
	}
	Log(static_cast<EngineLogLevel>(level), message);
}

void EngineLogger::Log(EngineLogLevel level, const char* message) noexcept
{
	if (message != nullptr) {
		WriteLogLine(level, message);
	}
}

void EngineLogger::Logf(EngineLogLevel level, const char* format, ...) noexcept
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

void EngineLogger::Trace(const char* message) noexcept
{
	Log(EngineLogLevel::Trace, message);
}

void EngineLogger::Debug(const char* message) noexcept
{
	Log(EngineLogLevel::Debug, message);
}

void EngineLogger::Info(const char* message) noexcept
{
	Log(EngineLogLevel::Info, message);
}

void EngineLogger::Warning(const char* message) noexcept
{
	Log(EngineLogLevel::Warning, message);
}

void EngineLogger::Error(const char* message) noexcept
{
	Log(EngineLogLevel::Error, message);
}

void EngineLogger::Critical(const char* message) noexcept
{
	Log(EngineLogLevel::Critical, message);
}
