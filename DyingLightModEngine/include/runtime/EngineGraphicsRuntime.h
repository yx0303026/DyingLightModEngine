#pragma once

#include "runtime/EngineInputRuntime.h"
#include "dl_imgui/dx11_hook.h"

#include <Windows.h>
#include <dxgi.h>

class EngineGraphicsRuntime final {
public:
	EngineGraphicsRuntime() = default;
	~EngineGraphicsRuntime();
	EngineGraphicsRuntime(const EngineGraphicsRuntime&) = delete;
	EngineGraphicsRuntime& operator=(const EngineGraphicsRuntime&) = delete;

	bool Initialize() noexcept;
	void Shutdown() noexcept;

private:
	bool CreateDummySwapChain() noexcept;
	void DestroyDummySwapChain() noexcept;
	void RenderFrame() noexcept;

	HWND window_ = nullptr;
	IDXGISwapChain* swapChain_ = nullptr;
	dl::imgui::Dx11Hook hook_;
	EngineInputRuntime input_;
	bool windowClassRegistered_ = false;
	bool initialized_ = false;
};
