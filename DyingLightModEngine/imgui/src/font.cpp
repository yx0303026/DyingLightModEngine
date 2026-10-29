#include "dl_imgui/font.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <imgui.h>

#include <cstddef>
#include <limits>

namespace dl::imgui::detail {
	namespace {

		constexpr std::size_t kFontPathCapacity = MAX_PATH;
		constexpr float kDefaultFontSizePixels = 18.0F;

		bool ResolveYaHeiFontPath(wchar_t(&fontPath)[kFontPathCapacity]) noexcept
		{
			const UINT directoryLength = GetWindowsDirectoryW(fontPath, static_cast<UINT>(kFontPathCapacity));
			if (directoryLength == 0U || directoryLength >= kFontPathCapacity) {
				return false;
			}

			constexpr wchar_t fontSuffix[] = L"\\Fonts\\msyh.ttc";
			constexpr std::size_t suffixLength = sizeof(fontSuffix) / sizeof(fontSuffix[0]);
			if (directoryLength + suffixLength > kFontPathCapacity) {
				return false;
			}

			if (wcscat_s(fontPath, kFontPathCapacity, fontSuffix) != 0) {
				return false;
			}

			const DWORD attributes = GetFileAttributesW(fontPath);
			return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U;
		}

	} // 匿名命名空间

	bool LoadDefaultChineseFont() noexcept
	{
		if (ImGui::GetCurrentContext() == nullptr) {
			return false;
		}

		wchar_t widePath[kFontPathCapacity]{};
		if (!ResolveYaHeiFontPath(widePath)) {
			return false;
		}

		HANDLE fontFile = CreateFileW(
			widePath,
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);
		if (fontFile == INVALID_HANDLE_VALUE) {
			return false;
		}

		LARGE_INTEGER fileSize{};
		if (GetFileSizeEx(fontFile, &fileSize) == 0 || fileSize.QuadPart <= 100 ||
			fileSize.QuadPart > std::numeric_limits<int>::max()) {
			CloseHandle(fontFile);
			return false;
		}

		const int fontDataSize = static_cast<int>(fileSize.QuadPart);
		void* fontData = ImGui::MemAlloc(static_cast<std::size_t>(fontDataSize));
		if (fontData == nullptr) {
			CloseHandle(fontFile);
			return false;
		}

		DWORD totalBytesRead = 0;
		while (totalBytesRead < static_cast<DWORD>(fontDataSize)) {
			DWORD bytesRead = 0;
			const DWORD remainingBytes = static_cast<DWORD>(fontDataSize) - totalBytesRead;
			if (ReadFile(fontFile, static_cast<unsigned char*>(fontData) + totalBytesRead, remainingBytes, &bytesRead, nullptr) == 0 ||
				bytesRead == 0U) {
				ImGui::MemFree(fontData);
				CloseHandle(fontFile);
				return false;
			}
			totalBytesRead += bytesRead;
		}
		CloseHandle(fontFile);

		ImFontConfig fontConfig{};
		ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
			fontData,
			fontDataSize,
			kDefaultFontSizePixels,
			&fontConfig,
			ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
		if (font == nullptr) {
			return false;
		}

		ImGui::GetIO().FontDefault = font;
		return true;
	}

} // namespace dl::imgui::detail
