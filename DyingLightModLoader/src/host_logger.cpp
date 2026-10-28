#include "host_logger.h"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace {

std::mutex g_logMutex;
HANDLE g_logFile = INVALID_HANDLE_VALUE;

const char* GetLogLevelName(uint32_t level) noexcept
{
    switch (static_cast<ModSDKLogLevel>(level)) {
    case ModSDKLogLevel::Trace:
        return "TRACE";
    case ModSDKLogLevel::Debug:
        return "DEBUG";
    case ModSDKLogLevel::Info:
        return "INFO";
    case ModSDKLogLevel::Warning:
        return "WARN";
    case ModSDKLogLevel::Error:
        return "ERROR";
    case ModSDKLogLevel::Critical:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

std::wstring GetLogFilePath(HMODULE loaderModule)
{
    std::array<wchar_t, 32768> modulePath{};
    const DWORD length = GetModuleFileNameW(loaderModule, modulePath.data(), static_cast<DWORD>(modulePath.size()));
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

}

bool InitializeHostLogger(HMODULE loaderModule) noexcept
{
    try {
        const std::wstring path = GetLogFilePath(loaderModule);
        if (path.empty()) {
            return false;
        }

        const std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_logFile != INVALID_HANDLE_VALUE) {
            return true;
        }

        g_logFile = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        return g_logFile != INVALID_HANDLE_VALUE;
    } catch (...) {
        return false;
    }
}

void MODSDK_CALL HostLog(void*, uint32_t level, const char* message) noexcept
{
    if (message == nullptr) {
        return;
    }

    SYSTEMTIME timestamp{};
    GetLocalTime(&timestamp);
    std::array<char, 256> prefix{};
    const int prefixLength = std::snprintf(prefix.data(), prefix.size(),
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
        WriteFile(g_logFile, prefix.data(), static_cast<DWORD>(prefixLength), &bytesWritten, nullptr);
        WriteFile(g_logFile, message, static_cast<DWORD>(std::strlen(message)), &bytesWritten, nullptr);
        WriteFile(g_logFile, "\r\n", 2, &bytesWritten, nullptr);
    } catch (...) {
    }
}
