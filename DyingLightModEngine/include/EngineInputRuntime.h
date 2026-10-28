#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>

class EngineInputRuntime final {
public:
	EngineInputRuntime() = default;
	~EngineInputRuntime();
	EngineInputRuntime(const EngineInputRuntime&) = delete;
	EngineInputRuntime& operator=(const EngineInputRuntime&) = delete;

	void AttachSwapChain(void* graphicsContext) noexcept;
	void RefreshUiState() noexcept;
	void Shutdown() noexcept;

private:
	static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	void AttachGameWindow(HWND window) noexcept;
	void DetachGameWindow() noexcept;
	LRESULT HandleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
	void ApplyUiInputState(bool uiOpen) noexcept;
	static bool IsBlockedInputMessage(UINT message) noexcept;

	HWND gameWindow_ = nullptr;
	WNDPROC originalWindowProc_ = nullptr;
	std::atomic_bool uiOpen_ = false;
	unsigned int cursorHideCallCount_ = 0;
	bool mouseCaptured_ = false;
	bool gameWindowHookInstalled_ = false;
};
