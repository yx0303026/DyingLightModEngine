#include "EngineInputRuntime.h"

#include "EngineModManager.h"

#include <dxgi.h>

namespace {

	const UINT kApplyUiInputStateMessage = WM_APP + 0x431U;
	EngineInputRuntime* g_activeRuntime = nullptr;

}

EngineInputRuntime::~EngineInputRuntime()
{
	Shutdown();
}

void EngineInputRuntime::AttachSwapChain(void* graphicsContext) noexcept
{
	auto* swapChain = static_cast<IDXGISwapChain*>(graphicsContext);
	if (swapChain == nullptr) {
		return;
	}
	DXGI_SWAP_CHAIN_DESC description{};
	if (SUCCEEDED(swapChain->GetDesc(&description)) &&
		description.OutputWindow != nullptr &&
		description.OutputWindow != gameWindow_) {
		AttachGameWindow(description.OutputWindow);
	}
}

void EngineInputRuntime::RefreshUiState() noexcept
{
	const bool uiOpen = EngineModManager::IsUiOpen();
	const bool previousState = uiOpen_.exchange(uiOpen);
	if (previousState != uiOpen && gameWindow_ != nullptr) {
		PostMessageW(gameWindow_, kApplyUiInputStateMessage, 0, 0);
	}
}

void EngineInputRuntime::Shutdown() noexcept
{
	DetachGameWindow();
}

LRESULT CALLBACK EngineInputRuntime::WindowProc(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	EngineInputRuntime* runtime = g_activeRuntime;
	if (runtime == nullptr || runtime->gameWindow_ != window ||
		runtime->originalWindowProc_ == nullptr) {
		return DefWindowProcW(window, message, wParam, lParam);
	}
	return runtime->HandleWindowMessage(window, message, wParam, lParam);
}

void EngineInputRuntime::AttachGameWindow(HWND window) noexcept
{
	if (window == nullptr || window == gameWindow_) {
		return;
	}
	DetachGameWindow();
	SetLastError(ERROR_SUCCESS);
	const LONG_PTR original = SetWindowLongPtrW(
		window,
		GWLP_WNDPROC,
		reinterpret_cast<LONG_PTR>(&WindowProc));
	if (original == 0 && GetLastError() != ERROR_SUCCESS) {
		return;
	}
	gameWindow_ = window;
	originalWindowProc_ = reinterpret_cast<WNDPROC>(original);
	gameWindowHookInstalled_ = true;
	g_activeRuntime = this;
}

void EngineInputRuntime::DetachGameWindow() noexcept
{
	if (!gameWindowHookInstalled_ || gameWindow_ == nullptr) {
		return;
	}
	const HWND gameWindow = gameWindow_;
	uiOpen_.store(false);
	const DWORD windowThread = GetWindowThreadProcessId(gameWindow, nullptr);
	if ((cursorHideCallCount_ != 0 || mouseCaptured_) &&
		windowThread != GetCurrentThreadId()) {
		SendMessageW(gameWindow, kApplyUiInputStateMessage, 0, 0);
	}
	if (GetWindowLongPtrW(gameWindow_, GWLP_WNDPROC) ==
		reinterpret_cast<LONG_PTR>(&WindowProc)) {
		SetWindowLongPtrW(
			gameWindow_,
			GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(originalWindowProc_));
	}
	if (g_activeRuntime == this) {
		g_activeRuntime = nullptr;
	}
	if (windowThread == GetCurrentThreadId()) {
		ApplyUiInputState(false);
	}
	gameWindow_ = nullptr;
	originalWindowProc_ = nullptr;
	gameWindowHookInstalled_ = false;
	cursorHideCallCount_ = 0;
	mouseCaptured_ = false;
}

LRESULT EngineInputRuntime::HandleWindowMessage(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam) noexcept
{
	if (message == kApplyUiInputStateMessage) {
		ApplyUiInputState(uiOpen_.load());
		return 0;
	}

	const bool uiOpen = EngineModManager::IsUiOpen();
	uiOpen_.store(uiOpen);
	ApplyUiInputState(uiOpen);
	EngineModManager::OnWindowMessage(
		reinterpret_cast<uintptr_t>(window),
		static_cast<uint32_t>(message),
		static_cast<uintptr_t>(wParam),
		static_cast<intptr_t>(lParam));

	if (uiOpen && message == WM_SETCURSOR) {
		SetCursor(nullptr);
		return TRUE;
	}
	if (uiOpen && IsBlockedInputMessage(message)) {
		return 0;
	}
	return CallWindowProcW(originalWindowProc_, window, message, wParam, lParam);
}

void EngineInputRuntime::ApplyUiInputState(bool uiOpen) noexcept
{
	if (gameWindow_ == nullptr) {
		return;
	}
	if (uiOpen) {
		if (cursorHideCallCount_ == 0) {
			int result = ShowCursor(FALSE);
			++cursorHideCallCount_;
			while (result >= 0) {
				result = ShowCursor(FALSE);
				++cursorHideCallCount_;
			}
		}
		SetCursor(nullptr);
		if (!mouseCaptured_) {
			SetCapture(gameWindow_);
			mouseCaptured_ = true;
		}
		return;
	}
	if (mouseCaptured_ && GetCapture() == gameWindow_) {
		ReleaseCapture();
	}
	mouseCaptured_ = false;
	while (cursorHideCallCount_ > 0) {
		ShowCursor(TRUE);
		--cursorHideCallCount_;
	}
}

bool EngineInputRuntime::IsBlockedInputMessage(UINT message) noexcept
{
	switch (message) {
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_XBUTTONDBLCLK:
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
	case WM_INPUT:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_DEADCHAR:
	case WM_SYSDEADCHAR:
	case WM_UNICHAR:
	case WM_IME_STARTCOMPOSITION:
	case WM_IME_ENDCOMPOSITION:
	case WM_IME_COMPOSITION:
	case WM_HOTKEY:
		return true;
	default:
		return false;
	}
}
