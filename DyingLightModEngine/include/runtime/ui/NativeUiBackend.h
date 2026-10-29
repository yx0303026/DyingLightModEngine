#pragma once

#include "runtime/ui/EngineUiTypes.h"

#include <cstdint>

namespace dl::engineui {

/* 原生 UI 回调概览：由游戏版本适配层填充，不暴露 ILevel/IUIElement 内部布局。 */
struct NativeUiResolver {
	void* context = nullptr;
	UiHandle (*getVisibleRoot)(void* context) = nullptr;
	UiHandle (*findElement)(void* context, UiHandle parent, const char* id) = nullptr;
	UiHandle (*cloneElement)(void* context, UiHandle source) = nullptr;
	UiHandle (*createText)(void* context) = nullptr;
	UiHandle (*createControl)(void* context) = nullptr;
	UiHandle (*createShape)(void* context) = nullptr;
	bool (*attach)(void* context, UiHandle child, UiHandle parent) = nullptr;
	bool (*setText)(void* context, UiHandle element, const char* text) = nullptr;
	bool (*setOptions)(void* context, UiHandle element, const UiElementOptions* options) = nullptr;
	bool (*setVisible)(void* context, UiHandle element, bool visible) = nullptr;
	void (*destroyRecursive)(void* context, UiHandle element) = nullptr;
};

/* 原生 UI 后端概览：在已有游戏屏幕上挂载可交互的无 PAK 元素。 */
class NativeUiBackend final {
public:
	NativeUiBackend() = default;
	~NativeUiBackend();
	NativeUiBackend(const NativeUiBackend&) = delete;
	NativeUiBackend& operator=(const NativeUiBackend&) = delete;

	bool Initialize(const NativeUiResolver& resolver) noexcept;
	void Shutdown() noexcept;
	bool IsInitialized() const noexcept;
	UiHandle CreateRoot() noexcept;
	UiHandle FindElement(UiHandle parent, const char* id) const noexcept;
	UiHandle CloneElement(UiHandle source) noexcept;
	UiHandle CreateText() noexcept;
	UiHandle CreateControl() noexcept;
	UiHandle CreateShape() noexcept;
	bool Attach(UiHandle child, UiHandle parent) noexcept;
	bool SetText(UiHandle element, const char* text) noexcept;
	bool SetOptions(UiHandle element, const UiElementOptions& options) noexcept;
	bool SetVisible(UiHandle element, bool visible) noexcept;
	void Destroy(UiHandle element) noexcept;

private:
	bool HasResolver() const noexcept;

	NativeUiResolver resolver_{};
	bool initialized_ = false;
};

} // namespace dl::engineui
