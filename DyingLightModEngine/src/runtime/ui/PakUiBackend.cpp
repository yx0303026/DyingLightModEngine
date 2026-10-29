#include "runtime/ui/PakUiBackend.h"

namespace dl::engineui {

/* 生命周期概览：PAK 后端负责脚本屏幕加载，但不复制游戏对象布局。 */
PakUiBackend::~PakUiBackend()
{
	Shutdown();
}

bool PakUiBackend::Initialize(const PakUiResolver& resolver) noexcept
{
	if (initialized_ || resolver.registerScreen == nullptr ||
		resolver.loadScreen == nullptr || resolver.showScreen == nullptr ||
		resolver.hideScreen == nullptr) {
		return false;
	}
	resolver_ = resolver;
	initialized_ = true;
	return true;
}

void PakUiBackend::Shutdown() noexcept
{
	resolver_ = {};
	initialized_ = false;
}

bool PakUiBackend::IsInitialized() const noexcept
{
	return initialized_;
}

std::int32_t PakUiBackend::RegisterScreen(const char* scriptPath, std::uint32_t flags) noexcept
{
	if (!HasResolver() || scriptPath == nullptr) {
		return -1;
	}
	return resolver_.registerScreen(resolver_.context, scriptPath, flags);
}

UiHandle PakUiBackend::LoadScreen(std::int32_t screenId) noexcept
{
	if (!HasResolver() || screenId < 0) {
		return 0;
	}
	return resolver_.loadScreen(resolver_.context, screenId);
}

bool PakUiBackend::ShowScreen(std::int32_t screenId) noexcept
{
	if (!HasResolver() || screenId < 0) {
		return false;
	}
	return resolver_.showScreen(resolver_.context, screenId);
}

bool PakUiBackend::HideScreen(std::int32_t screenId) noexcept
{
	if (!HasResolver() || screenId < 0) {
		return false;
	}
	return resolver_.hideScreen(resolver_.context, screenId);
}

UiHandle PakUiBackend::FindElement(UiHandle screen, const char* id) const noexcept
{
	if (!HasResolver() || resolver_.findElement == nullptr || screen == 0 || id == nullptr) {
		return 0;
	}
	return resolver_.findElement(resolver_.context, screen, id);
}

bool PakUiBackend::SetText(UiHandle element, const char* text) noexcept
{
	if (!HasResolver() || resolver_.setText == nullptr || element == 0 || text == nullptr) {
		return false;
	}
	return resolver_.setText(resolver_.context, element, text);
}

bool PakUiBackend::SetOptions(UiHandle element, const UiElementOptions& options) noexcept
{
	if (!HasResolver() || resolver_.setOptions == nullptr || element == 0) {
		return false;
	}
	return resolver_.setOptions(resolver_.context, element, &options);
}

void PakUiBackend::DestroyScreen(UiHandle screen) noexcept
{
	if (HasResolver() && resolver_.destroyScreen != nullptr && screen != 0) {
		resolver_.destroyScreen(resolver_.context, screen);
	}
}

bool PakUiBackend::HasResolver() const noexcept
{
	return initialized_ && resolver_.context != nullptr;
}

} // namespace dl::engineui
