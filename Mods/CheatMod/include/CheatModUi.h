#pragma once

#include "dl_imgui/imgui.h"

#include "EngineAbi.h"

#include <d3d11.h>
#include <dxgi.h>

#include <functional>
#include <memory>
#include <atomic>
#include <cstdint>

namespace cheat_mod {

	class CheatModUi final {
	public:
		CheatModUi() = default;
		~CheatModUi() = default;
		CheatModUi(const CheatModUi&) = delete;
		CheatModUi& operator=(const CheatModUi&) = delete;

		bool Initialize();
		void Shutdown() noexcept;
		void Render(void* graphicsContext);
		void HandleWindowMessage(
			std::uintptr_t window,
			std::uint32_t message,
			std::uintptr_t wParam,
			std::intptr_t lParam) noexcept;
		bool IsOpen() const noexcept;
		void SetOpen(bool open) noexcept;
		void Toggle() noexcept;
		void SetMouseStateProvider(ModEngineGetUiMouseStateFn provider) noexcept;

	private:
		void BuildNavigation();
		void BuildDashboardPage(dl::imgui::Page& page);
		void BuildPlayerPage(dl::imgui::Page& page);
		void BuildSettingsPage(dl::imgui::Page& page);
		void OnGodModeChanged(bool enabled) noexcept;
		void OnInfiniteStaminaChanged(bool enabled) noexcept;
		void OnUiScaleChanged(float scale) noexcept;
		bool InitializeGraphics(void* graphicsContext) noexcept;
		void ShutdownGraphics() noexcept;
		bool CreateRenderTarget() noexcept;
		static bool IsMouseMessage(std::uint32_t message) noexcept;

		dl::imgui::UiContext context_;
		std::unique_ptr<dl::imgui::NavigationView> navigation_;
		IDXGISwapChain* swapChain_ = nullptr;
		ID3D11Device* device_ = nullptr;
		ID3D11DeviceContext* deviceContext_ = nullptr;
		ID3D11RenderTargetView* renderTarget_ = nullptr;
		HWND window_ = nullptr;
		std::atomic_bool open_ = false;
		std::atomic_bool engineMouse_ = false;
		bool graphicsInitialized_ = false;
		bool godModeEnabled_ = false;
		bool infiniteStaminaEnabled_ = false;
		float playerSpeed_ = 1.0F;
		float uiScale_ = 1.0F;
	};

} // namespace cheat_mod
