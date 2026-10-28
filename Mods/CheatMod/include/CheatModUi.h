#pragma once

#include "dl_imgui/imgui.h"

#include <d3d11.h>
#include <dxgi.h>

#include <functional>
#include <memory>

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
		bool IsOpen() const noexcept;
		void SetOpen(bool open) noexcept;
		void Toggle() noexcept;

	private:
		void BuildNavigation();
		void BuildDashboardPage(dl::imgui::Page& page);
		void BuildPlayerPage(dl::imgui::Page& page);
		void BuildSettingsPage(dl::imgui::Page& page);
		void OnGodModeChanged(bool enabled) noexcept;
		void OnInfiniteStaminaChanged(bool enabled) noexcept;
		bool InitializeGraphics(void* graphicsContext) noexcept;
		void ShutdownGraphics() noexcept;
		bool CreateRenderTarget() noexcept;

		dl::imgui::UiContext context_;
		std::unique_ptr<dl::imgui::NavigationView> navigation_;
		IDXGISwapChain* swapChain_ = nullptr;
		ID3D11Device* device_ = nullptr;
		ID3D11DeviceContext* deviceContext_ = nullptr;
		ID3D11RenderTargetView* renderTarget_ = nullptr;
		HWND window_ = nullptr;
		bool open_ = false;
		bool graphicsInitialized_ = false;
		bool godModeEnabled_ = false;
		bool infiniteStaminaEnabled_ = false;
		float playerSpeed_ = 1.0F;
	};

} // namespace cheat_mod
