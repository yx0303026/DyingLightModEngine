#include "logging/EngineLogger.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>

namespace {

struct ModLogContext {
	std::string modName;
};

std::mutex g_logMutex;
std::filesystem::path g_gameRoot;
std::wstring g_logFileName;

const char* GetLogLevelName(EngineLogLevel level) noexcept
{
	switch (level) {
	case EngineLogLevel::Trace: return "TRACE";
	case EngineLogLevel::Debug: return "DEBUG";
	case EngineLogLevel::Info: return "INFO";
	case EngineLogLevel::Warning: return "WARN";
	case EngineLogLevel::Error: return "ERROR";
	case EngineLogLevel::Critical: return "CRITICAL";
	default: return "UNKNOWN";
	}
}

std::filesystem::path GetModuleDirectory(HMODULE module)
{
	std::array<wchar_t, 32768> buffer{};
	const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
	if (length == 0 || length >= buffer.size()) return {};
	return std::filesystem::path(buffer.data(), buffer.data() + length).parent_path();
}

std::wstring ToWide(const char* value)
{
	if (value == nullptr || value[0] == '\0') return {};
	const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, nullptr, 0);
	if (required <= 1) return {};
	std::wstring result(static_cast<size_t>(required), L'\0');
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, result.data(), required) <= 0) return {};
	result.resize(static_cast<size_t>(required - 1));
	return result;
}

bool IsSafeName(const std::wstring& value) noexcept
{
	if (value.empty() || value == L"." || value == L"..") return false;
	for (const wchar_t character : value) {
		if (character == L'/' || character == L'\\' || character == L':') return false;
	}
	return true;
}

std::filesystem::path GetLogPath(const char* modName)
{
	const std::wstring wideName = ToWide(modName);
	if (!IsSafeName(wideName)) return {};
	std::lock_guard<std::mutex> lock(g_logMutex);
	return g_gameRoot.empty() || g_logFileName.empty()
		? std::filesystem::path{}
		: g_gameRoot / L"Logs" / wideName / g_logFileName;
}

void WriteLogLine(const std::filesystem::path& path, EngineLogLevel level, const char* message) noexcept
{
	SYSTEMTIME timestamp{};
	GetLocalTime(&timestamp);
	std::array<char, 256> prefix{};
	const int prefixLength = std::snprintf(prefix.data(), prefix.size(),
		"[%04u-%02u-%02u %02u:%02u:%02u.%03u] [%s] ",
		static_cast<unsigned int>(timestamp.wYear), static_cast<unsigned int>(timestamp.wMonth),
		static_cast<unsigned int>(timestamp.wDay), static_cast<unsigned int>(timestamp.wHour),
		static_cast<unsigned int>(timestamp.wMinute), static_cast<unsigned int>(timestamp.wSecond),
		static_cast<unsigned int>(timestamp.wMilliseconds), GetLogLevelName(level));
	if (prefixLength <= 0 || static_cast<size_t>(prefixLength) >= prefix.size()) return;
	OutputDebugStringA(prefix.data());
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
	try {
		std::error_code error;
		std::filesystem::create_directories(path.parent_path(), error);
		if (error) return;
		const HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE) return;
		DWORD written = 0;
		WriteFile(file, prefix.data(), static_cast<DWORD>(prefixLength), &written, nullptr);
		WriteFile(file, message, static_cast<DWORD>(std::strlen(message)), &written, nullptr);
		WriteFile(file, "\r\n", 2, &written, nullptr);
		CloseHandle(file);
	}
	catch (...) {
	}
}

}

bool EngineLogger::Initialize(HMODULE engineModule) noexcept
{
	if (engineModule == nullptr) return false;
	try {
		const std::filesystem::path root = GetModuleDirectory(engineModule);
		if (root.empty()) return false;
		SYSTEMTIME timestamp{};
		GetLocalTime(&timestamp);
		std::array<wchar_t, 32> fileName{};
		const int length = std::swprintf(fileName.data(), fileName.size(),
			L"%04u-%02u-%02u_%02u-%02u-%02u.log",
			static_cast<unsigned int>(timestamp.wYear), static_cast<unsigned int>(timestamp.wMonth),
			static_cast<unsigned int>(timestamp.wDay), static_cast<unsigned int>(timestamp.wHour),
			static_cast<unsigned int>(timestamp.wMinute), static_cast<unsigned int>(timestamp.wSecond));
		if (length <= 0 || static_cast<size_t>(length) >= fileName.size()) return false;
		const std::filesystem::path logPath = root / L"Logs" / L"DyingLightModEngine" / fileName.data();
		std::error_code error;
		std::filesystem::create_directories(logPath.parent_path(), error);
		if (error) return false;

		const HANDLE file = CreateFileW(
			logPath.c_str(),
			FILE_APPEND_DATA,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);
		if (file == INVALID_HANDLE_VALUE) return false;
		CloseHandle(file);

		std::lock_guard<std::mutex> lock(g_logMutex);
		g_gameRoot = root;
		g_logFileName = fileName.data();
		return true;
	}
	catch (...) {
		return false;
	}
}

void* EngineLogger::CreateModContext(const char* modName) noexcept
{
	try {
		const std::wstring wideName = ToWide(modName);
		if (!IsSafeName(wideName)) return nullptr;
		return new ModLogContext{ modName };
	}
	catch (...) {
		return nullptr;
	}
}

void EngineLogger::DestroyModContext(void* context) noexcept
{
	delete static_cast<ModLogContext*>(context);
}

void MODSDK_CALL EngineLogger::HostCallback(void* context, uint32_t level, const char* message) noexcept
{
	if (message == nullptr || level > static_cast<uint32_t>(EngineLogLevel::Critical)) return;
	const auto* modContext = static_cast<const ModLogContext*>(context);
	const char* modName = modContext == nullptr ? "DyingLightModEngine" : modContext->modName.c_str();
	const std::filesystem::path path = GetLogPath(modName);
	if (!path.empty()) {
		std::lock_guard<std::mutex> lock(g_logMutex);
		WriteLogLine(path, static_cast<EngineLogLevel>(level), message);
	}
}

void EngineLogger::Log(EngineLogLevel level, const char* message) noexcept
{
	if (message == nullptr) return;
	const std::filesystem::path path = GetLogPath("DyingLightModEngine");
	if (!path.empty()) {
		std::lock_guard<std::mutex> lock(g_logMutex);
		WriteLogLine(path, level, message);
	}
}

void EngineLogger::Logf(EngineLogLevel level, const char* format, ...) noexcept
{
	if (format == nullptr) return;
	char message[2048]{};
	va_list arguments;
	va_start(arguments, format);
	_vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
	va_end(arguments);
	Log(level, message);
}

void EngineLogger::Trace(const char* message) noexcept { Log(EngineLogLevel::Trace, message); }
void EngineLogger::Debug(const char* message) noexcept { Log(EngineLogLevel::Debug, message); }
void EngineLogger::Info(const char* message) noexcept { Log(EngineLogLevel::Info, message); }
void EngineLogger::Warning(const char* message) noexcept { Log(EngineLogLevel::Warning, message); }
void EngineLogger::Error(const char* message) noexcept { Log(EngineLogLevel::Error, message); }
void EngineLogger::Critical(const char* message) noexcept { Log(EngineLogLevel::Critical, message); }
