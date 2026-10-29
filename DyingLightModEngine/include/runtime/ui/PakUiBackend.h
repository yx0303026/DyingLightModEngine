#pragma once

#include "runtime/ui/EngineUiTypes.h"

#include <cstdint>

namespace dl::engineui {

/* PAK UI 回调概览：把 RegisterUIScreen/LoadUIScreen 等游戏接口隐藏在适配层。 */
struct PakUiResolver {
	void* context = nullptr;
	std::int32_t (*registerScreen)(void* context, const char* scriptPath, std::uint32_t flags) = nullptr;
	UiHandle (*loadScreen)(void* context, std::int32_t screenId) = nullptr;
	bool (*showScreen)(void* context, std::int32_t screenId) = nullptr;
	bool (*hideScreen)(void* context, std::int32_t screenId) = nullptr;
	UiHandle (*findElement)(void* context, UiHandle screen, const char* id) = nullptr;
	bool (*setText)(void* context, UiHandle element, const char* text) = nullptr;
	bool (*setOptions)(void* context, UiHandle element, const UiElementOptions* options) = nullptr;
	void (*destroyScreen)(void* context, UiHandle screen) = nullptr;
};

/* PAK UI 后端概览：加载模组自定义 XUI/资源并维护屏幕生命周期。 */
class PakUiBackend final {
public:
	PakUiBackend() = default;
	~PakUiBackend();
	PakUiBackend(const PakUiBackend&) = delete;
	PakUiBackend& operator=(const PakUiBackend&) = delete;

	bool Initialize(const PakUiResolver& resolver) noexcept;
	void Shutdown() noexcept;
	bool IsInitialized() const noexcept;
	std::int32_t RegisterScreen(const char* scriptPath, std::uint32_t flags) noexcept;
	UiHandle LoadScreen(std::int32_t screenId) noexcept;
	bool ShowScreen(std::int32_t screenId) noexcept;
	bool HideScreen(std::int32_t screenId) noexcept;
	UiHandle FindElement(UiHandle screen, const char* id) const noexcept;
	bool SetText(UiHandle element, const char* text) noexcept;
	bool SetOptions(UiHandle element, const UiElementOptions& options) noexcept;
	void DestroyScreen(UiHandle screen) noexcept;

private:
	bool HasResolver() const noexcept;

	PakUiResolver resolver_{};
	bool initialized_ = false;
};

} // namespace dl::engineui
