#include "CheatModUi.h"

#include <Windows.h>

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <functional>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam);

namespace cheat_mod {

	bool CheatModUi::Initialize()
	{
		if (navigation_ != nullptr || !context_.Create()) {
			return false;
		}

		ImGui::SetCurrentContext(context_.GetNativeContext());
		/* 引擎鼠标事件在 Win32 后端读取冻结系统光标后注入，因此同一帧内以后注入的事件优先。 */
		ImGui::GetIO().ConfigInputTrickleEventQueue = false;

		navigation_ = std::make_unique<dl::imgui::NavigationView>(180.0F);
		BuildNavigation();
		return true;
	}

	void CheatModUi::Shutdown() noexcept
	{
		ShutdownGraphics();
		navigation_.reset();
		context_.Destroy();
		open_.store(false);
	}

	void CheatModUi::Render(void* graphicsContext)
	{
		if (navigation_ == nullptr || !context_.IsCreated()) {
			return;
		}

		ImGui::SetCurrentContext(context_.GetNativeContext());
		if (!InitializeGraphics(graphicsContext)) {
			return;
		}
		lobby_.Refresh();
		RefreshLobbyLabels();
		bool open = open_.load();
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		context_.BeginFrame();
		engineMouse_.store(open && context_.IsMouseProviderActive());
		if (open) {
			ImGui::SetNextWindowSize(dl::imgui::ScaleSize(ImVec2(760.0F, 520.0F)), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Cheat Mod", &open, ImGuiWindowFlags_NoCollapse)) {
				context_.Render(*navigation_);
			}
			ImGui::End();
		}
		open_.store(open);
		context_.EndFrame();
		deviceContext_->OMSetRenderTargets(1, &renderTarget_, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	bool CheatModUi::IsOpen() const noexcept
	{
		return open_.load();
	}

	void CheatModUi::SetOpen(bool open) noexcept
	{
		open_.store(open);
	}

	void CheatModUi::Toggle() noexcept
	{
		open_.exchange(!open_.load());
	}

	void CheatModUi::SetMouseStateProvider(ModEngineGetUiMouseStateFn provider) noexcept
	{
		context_.SetMouseStateProvider(provider);
	}

	void CheatModUi::SetLobbyApi(const DlModLobbyApiV1* api) noexcept
	{
		lobby_.SetApi(api);
	}

	void CheatModUi::HandleWindowMessage(
		std::uintptr_t window,
		std::uint32_t message,
		std::uintptr_t wParam,
		std::intptr_t lParam) noexcept
	{
		if (!graphicsInitialized_ || !context_.IsCreated()) {
			return;
		}
		if (open_.load() && engineMouse_.load() && IsMouseMessage(message)) {
			/* 游戏光标冻结期间由 DirectInput 桥接层接管指针输入。 */
			return;
		}
		ImGui::SetCurrentContext(context_.GetNativeContext());
		ImGui_ImplWin32_WndProcHandler(
			reinterpret_cast<HWND>(window),
			static_cast<UINT>(message),
			static_cast<WPARAM>(wParam),
			static_cast<LPARAM>(lParam));
	}

	bool CheatModUi::IsMouseMessage(std::uint32_t message) noexcept
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
			return true;
		default:
			return false;
		}
	}

	void CheatModUi::BuildNavigation()
	{
		dl::imgui::Page& dashboard = navigation_->AddPage("dashboard", "Dashboard");
		dl::imgui::Page& player = navigation_->AddPage("player", "Player");
		dl::imgui::Page& roomList = navigation_->AddPage("room-list", "Room List");
		dl::imgui::Page& settings = navigation_->AddPage("settings", "Settings");
		navigation_->GetNavigationBar().SetItemIcon("dashboard", dl::imgui::NavigationIcon::Home);
		navigation_->GetNavigationBar().SetItemIcon("player", dl::imgui::NavigationIcon::Person);
		navigation_->GetNavigationBar().SetItemIcon("room-list", dl::imgui::NavigationIcon::Game);
		navigation_->GetNavigationBar().SetItemIcon("settings", dl::imgui::NavigationIcon::Settings);
		BuildDashboardPage(dashboard);
		BuildPlayerPage(player);
		BuildRoomListPage(roomList);
		BuildSettingsPage(settings);
	}

	void CheatModUi::BuildDashboardPage(dl::imgui::Page& page)
	{
		page.AddChild<dl::imgui::Label>("Cheat Mod");
		page.AddChild<dl::imgui::Label>("Press F2 to toggle this menu.");
		page.AddChild<dl::imgui::Separator>();
		dl::imgui::CheckBox& godMode = page.AddChild<dl::imgui::CheckBox>("God mode", godModeEnabled_);
		godMode.SetOnChanged(std::bind(&CheatModUi::OnGodModeChanged, this, std::placeholders::_1));
		dl::imgui::CheckBox& infiniteStamina = page.AddChild<dl::imgui::CheckBox>("Infinite stamina", infiniteStaminaEnabled_);
		infiniteStamina.SetOnChanged(std::bind(&CheatModUi::OnInfiniteStaminaChanged, this, std::placeholders::_1));
	}

	void CheatModUi::BuildPlayerPage(dl::imgui::Page& page)
	{
		page.AddChild<dl::imgui::Label>("Player");
		page.AddChild<dl::imgui::Slider>("Movement speed", playerSpeed_, 0.25F, 3.0F);
		page.AddChild<dl::imgui::Label>("Player controls are kept in this page so new features can be added independently.");
	}

	void CheatModUi::BuildRoomListPage(dl::imgui::Page& page)
	{
		page.AddChild<dl::imgui::Label>("Room members and search results");
		lobbyStatusLabel_ = &page.AddChild<dl::imgui::Label>(lobby_.GetStatusText());
		page.AddChild<dl::imgui::Separator>();
		dl::imgui::ScrollView& currentScroll = page.AddChild<dl::imgui::ScrollView>(ImVec2(0.0F, 220.0F), true);
		lobbyCurrentLabel_ = &currentScroll.AddChild<dl::imgui::Label>("Current members: ");
		dl::imgui::ScrollView& historyScroll = page.AddChild<dl::imgui::ScrollView>(ImVec2(0.0F, 180.0F), true);
		lobbyHistoryLabel_ = &historyScroll.AddChild<dl::imgui::Label>("History members: ");
		dl::imgui::ScrollView& roomsScroll = page.AddChild<dl::imgui::ScrollView>(ImVec2(0.0F, 180.0F), true);
		lobbyRoomsLabel_ = &roomsScroll.AddChild<dl::imgui::Label>("Search results: ");
		dl::imgui::Button& clearHistory = page.AddChild<dl::imgui::Button>("Clear history");
		clearHistory.SetOnClick(std::bind(&CheatModUi::OnClearLobbyHistory, this));
		dl::imgui::Button& copyMembers = page.AddChild<dl::imgui::Button>("Copy current members");
		copyMembers.SetOnClick(std::bind(&CheatModUi::OnCopyLobbyMembers, this));
	}

	void CheatModUi::BuildSettingsPage(dl::imgui::Page& page)
	{
		page.AddChild<dl::imgui::Label>("Settings");
		page.AddChild<dl::imgui::Label>("Cheat Mod UI is rendered through the Engine ImGui facade.");
		dl::imgui::Slider& uiScale = page.AddChild<dl::imgui::Slider>("UI scale", uiScale_, 0.5F, 2.0F);
		uiScale.SetOnChanged(std::bind(&CheatModUi::OnUiScaleChanged, this, std::placeholders::_1));
	}

	void CheatModUi::OnGodModeChanged(bool enabled) noexcept
	{
		godModeEnabled_ = enabled;
	}

	void CheatModUi::OnInfiniteStaminaChanged(bool enabled) noexcept
	{
		infiniteStaminaEnabled_ = enabled;
	}

	void CheatModUi::OnClearLobbyHistory() noexcept
	{
		lobby_.ClearHistory();
	}

	void CheatModUi::OnCopyLobbyMembers() noexcept
	{
		lobby_.CopyCurrentToClipboard();
	}

	void CheatModUi::OnUiScaleChanged(float scale) noexcept
	{
		uiScale_ = scale;
		context_.SetScale(scale);
	}

	void CheatModUi::RefreshLobbyLabels() noexcept
	{
		if (lobbyStatusLabel_ != nullptr)
			lobbyStatusLabel_->SetText(lobby_.GetStatusText());
		if (lobbyCurrentLabel_ != nullptr)
			lobbyCurrentLabel_->SetText(std::string("Current members: ") + lobby_.GetCurrentText());
		if (lobbyHistoryLabel_ != nullptr)
			lobbyHistoryLabel_->SetText(std::string("History members: ") + lobby_.GetHistoryText());
		if (lobbyRoomsLabel_ != nullptr)
			lobbyRoomsLabel_->SetText(std::string("Search results: ") + lobby_.GetRoomsText());
	}

	bool CheatModUi::InitializeGraphics(void* graphicsContext) noexcept
	{
		auto* swapChain = static_cast<IDXGISwapChain*>(graphicsContext);
		if (swapChain == nullptr) {
			return false;
		}
		if (graphicsInitialized_ && swapChain_ == swapChain) {
			return true;
		}

		ShutdownGraphics();
		DXGI_SWAP_CHAIN_DESC description{};
		if (FAILED(swapChain->GetDesc(&description)) ||
			FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device_))) ||
			device_ == nullptr) {
			ShutdownGraphics();
			return false;
		}
		device_->GetImmediateContext(&deviceContext_);
		if (deviceContext_ == nullptr || !ImGui_ImplWin32_Init(description.OutputWindow)) {
			ShutdownGraphics();
			return false;
		}
		if (!ImGui_ImplDX11_Init(device_, deviceContext_)) {
			ImGui_ImplWin32_Shutdown();
			ShutdownGraphics();
			return false;
		}

		swapChain_ = swapChain;
		swapChain_->AddRef();
		window_ = description.OutputWindow;
		graphicsInitialized_ = true;
		if (!CreateRenderTarget()) {
			ShutdownGraphics();
			return false;
		}
		return true;
	}

	void CheatModUi::ShutdownGraphics() noexcept
	{
		if (graphicsInitialized_) {
			ImGui::SetCurrentContext(context_.GetNativeContext());
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
		}
		if (renderTarget_ != nullptr) {
			renderTarget_->Release();
			renderTarget_ = nullptr;
		}
		if (deviceContext_ != nullptr) {
			deviceContext_->Release();
			deviceContext_ = nullptr;
		}
		if (device_ != nullptr) {
			device_->Release();
			device_ = nullptr;
		}
		if (swapChain_ != nullptr) {
			swapChain_->Release();
			swapChain_ = nullptr;
		}
		window_ = nullptr;
		graphicsInitialized_ = false;
	}

	bool CheatModUi::CreateRenderTarget() noexcept
	{
		if (swapChain_ == nullptr || device_ == nullptr) {
			return false;
		}
		ID3D11Texture2D* backBuffer = nullptr;
		const HRESULT bufferResult = swapChain_->GetBuffer(
			0,
			__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&backBuffer));
		if (FAILED(bufferResult) || backBuffer == nullptr) {
			return false;
		}
		const HRESULT targetResult = device_->CreateRenderTargetView(backBuffer, nullptr, &renderTarget_);
		backBuffer->Release();
		return SUCCEEDED(targetResult) && renderTarget_ != nullptr;
	}

} // cheat_mod 命名空间
