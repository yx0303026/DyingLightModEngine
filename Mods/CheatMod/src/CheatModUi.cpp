#include "CheatModUi.h"

#include <Windows.h>

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <functional>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam);

namespace cheat_mod {

	CheatModUi::CheatModUi()
		: lobbyView_(lobby_)
	{
	}

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
		lobbyView_.Refresh();
		bool open = open_.load();
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		context_.BeginFrame();
		if (bulletTracking_ != nullptr) {
			bulletTracking_->Refresh();
			bulletTracking_->DrawOverlay();
		}
		engineMouse_.store(open && context_.IsMouseProviderActive());
		if (open) {
			UpdateTrackingLabels();
			ImGui::SetNextWindowSize(dl::imgui::ScaleSize(ImVec2(980.0F, 640.0F)), ImGuiCond_FirstUseEver);
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

	void CheatModUi::SetBulletTracking(CheatModBulletTracking* tracking) noexcept
	{
		bulletTracking_ = tracking;
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
		const ImVec2 pagePadding(8.0F, 8.0F);
		dl::imgui::Page& dashboard = navigation_->AddPage(
			"dashboard",
			"Dashboard",
			"Overview of the active mod session",
			pagePadding);
		dl::imgui::Page& player = navigation_->AddPage(
			"player",
			"Player",
			"Inspect and control the local player",
			pagePadding);
		dl::imgui::Page& bulletTracking = navigation_->AddPage(
			"bullet-tracking",
			"Bullet Tracking",
			"Lock a target and steer projectiles toward it",
			pagePadding);
		dl::imgui::Page& roomList = navigation_->AddPage(
			"room-list",
			"Room List",
			"Browse rooms discovered by the lobby service",
			pagePadding);
		dl::imgui::Page& roomMembers = navigation_->AddPage(
			"room-members",
			"Room Members",
			"Inspect members in the selected room",
			pagePadding);
		dl::imgui::Page& settings = navigation_->AddPage(
			"settings",
			"Settings",
			"Configure mod behavior and runtime options",
			pagePadding);
		navigation_->GetNavigationBar().SetItemIcon("dashboard", dl::imgui::NavigationIcon::Home);
		navigation_->GetNavigationBar().SetItemIcon("player", dl::imgui::NavigationIcon::Person);
		navigation_->GetNavigationBar().SetItemIcon("bullet-tracking", dl::imgui::NavigationIcon::Tools);
		navigation_->GetNavigationBar().SetItemIcon("room-list", dl::imgui::NavigationIcon::Game);
		navigation_->GetNavigationBar().SetItemIcon("room-members", dl::imgui::NavigationIcon::Person);
		navigation_->GetNavigationBar().SetItemIcon("settings", dl::imgui::NavigationIcon::Settings);
		BuildDashboardPage(dashboard);
		BuildPlayerPage(player);
		BuildBulletTrackingPage(bulletTracking);
		lobbyView_.BuildRoomListPage(roomList);
		lobbyView_.BuildMembersPage(roomMembers);
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

	void CheatModUi::BuildBulletTrackingPage(dl::imgui::Page& page)
	{
		page.AddChild<dl::imgui::Label>("Steers arrows, bolts and bullets toward the locked target.");
		if (bulletTracking_ == nullptr || !bulletTracking_->IsAvailable()) {
			page.AddChild<dl::imgui::Label>("The engine projectile API is unavailable in this build.");
			return;
		}

		dl::imgui::CheckBox& enabled = page.AddChild<dl::imgui::CheckBox>(
			"Enable bullet tracking", bulletTracking_->IsEnabled());
		enabled.SetOnChanged([this](bool value) {
			bulletTracking_->SetEnabled(value);
		});
		dl::imgui::CheckBox& showTarget = page.AddChild<dl::imgui::CheckBox>(
			"Show target marker", bulletTracking_->IsShowTarget());
		showTarget.SetOnChanged([this](bool value) {
			bulletTracking_->SetShowTarget(value);
		});

		page.AddChild<dl::imgui::Separator>();
		dl::imgui::ComboBox& lockMode = page.AddChild<dl::imgui::ComboBox>("Lock mode");
		lockMode.SetItems({ "Body", "Head" });
		lockMode.SetSelectedIndex(bulletTracking_->GetLockMode());
		lockMode.SetOnSelectionChanged([this](int index, const std::string&) {
			bulletTracking_->SetLockMode(index);
		});

		dl::imgui::Slider& precision = page.AddChild<dl::imgui::Slider>(
			"Precision", bulletTracking_->GetPrecision(), 0.0F, 1.0F);
		precision.SetOnChanged([this](float value) {
			bulletTracking_->SetPrecision(value);
		});
		dl::imgui::Slider& turnRate = page.AddChild<dl::imgui::Slider>(
			"Turn rate", bulletTracking_->GetTurnRate(), 0.1F, 30.0F);
		turnRate.SetOnChanged([this](float value) {
			bulletTracking_->SetTurnRate(value);
		});
		dl::imgui::Slider& turnRadius = page.AddChild<dl::imgui::Slider>(
			"Turn radius", bulletTracking_->GetTurnRadius(), 0.0F, 200.0F);
		turnRadius.SetOnChanged([this](float value) {
			bulletTracking_->SetTurnRadius(value);
		});
		dl::imgui::Slider& maximumDistance = page.AddChild<dl::imgui::Slider>(
			"Maximum distance", bulletTracking_->GetMaximumDistance(), 10.0F, 300.0F);
		maximumDistance.SetOnChanged([this](float value) {
			bulletTracking_->SetMaximumDistance(value);
		});
		dl::imgui::Slider& fovRadius = page.AddChild<dl::imgui::Slider>(
			"FOV radius (px)", bulletTracking_->GetFovRadius(), 0.0F, 800.0F);
		fovRadius.SetOnChanged([this](float value) {
			bulletTracking_->SetFovRadius(value);
		});

		page.AddChild<dl::imgui::Separator>();
		dl::imgui::CheckBox& humans = page.AddChild<dl::imgui::CheckBox>(
			"Target humans", bulletTracking_->IsTargetKindEnabled(0));
		humans.SetOnChanged([this](bool value) {
			bulletTracking_->SetTargetKind(0, value);
		});
		dl::imgui::CheckBox& hunters = page.AddChild<dl::imgui::CheckBox>(
			"Target night hunters", bulletTracking_->IsTargetKindEnabled(1));
		hunters.SetOnChanged([this](bool value) {
			bulletTracking_->SetTargetKind(1, value);
		});
		dl::imgui::CheckBox& zombies = page.AddChild<dl::imgui::CheckBox>(
			"Target infected", bulletTracking_->IsTargetKindEnabled(2));
		zombies.SetOnChanged([this](bool value) {
			bulletTracking_->SetTargetKind(2, value);
		});
		dl::imgui::CheckBox& arrows = page.AddChild<dl::imgui::CheckBox>(
			"Steer arrows", bulletTracking_->IsProjectileKindEnabled(0));
		arrows.SetOnChanged([this](bool value) {
			bulletTracking_->SetProjectileKind(0, value);
		});
		dl::imgui::CheckBox& bolts = page.AddChild<dl::imgui::CheckBox>(
			"Steer bolts", bulletTracking_->IsProjectileKindEnabled(1));
		bolts.SetOnChanged([this](bool value) {
			bulletTracking_->SetProjectileKind(1, value);
		});
		dl::imgui::CheckBox& bullets = page.AddChild<dl::imgui::CheckBox>(
			"Steer bullets", bulletTracking_->IsProjectileKindEnabled(2));
		bullets.SetOnChanged([this](bool value) {
			bulletTracking_->SetProjectileKind(2, value);
		});

		page.AddChild<dl::imgui::Separator>();
		trackingStatusLabel_ = &page.AddChild<dl::imgui::Label>(bulletTracking_->GetStatusText());
		trackingStatsLabel_ = &page.AddChild<dl::imgui::Label>("");
	}

	void CheatModUi::UpdateTrackingLabels()
	{
		if (bulletTracking_ == nullptr) {
			return;
		}
		if (trackingStatusLabel_ != nullptr) {
			trackingStatusLabel_->SetText(std::string("Status: ") + bulletTracking_->GetStatusText());
		}
		if (trackingStatsLabel_ != nullptr) {
			const BulletTrackingStats& stats = bulletTracking_->GetStats();
			trackingStatsLabel_->SetText(
				"Updates " + std::to_string(stats.updates) +
				" | Candidates " + std::to_string(stats.candidates) +
				" | Adjusted " + std::to_string(stats.adjusted) +
				" | Targets " + std::to_string(stats.targetsFound) +
				"\nNearby " + std::to_string(stats.nearby) +
				" | Kind " + std::to_string(stats.kindMatches) +
				" | Bone " + std::to_string(stats.boneMatches) +
				" | Proj " + std::to_string(stats.projected));
		}
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

	void CheatModUi::OnUiScaleChanged(float scale) noexcept
	{
		uiScale_ = scale;
		context_.SetScale(scale);
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
