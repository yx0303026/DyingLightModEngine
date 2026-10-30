#include "runtime/EngineGraphicsRuntime.h"

#include "managers/EngineModManager.h"

#include <d3d11.h>
#include <functional>

namespace
{

	const wchar_t *kWindowClassName = L"DyingLightModEngine.GraphicsRuntime";

}

EngineGraphicsRuntime::~EngineGraphicsRuntime()
{
	Shutdown();
}

bool EngineGraphicsRuntime::Initialize() noexcept
{
	if (initialized_)
	{
		return true;
	}
	if (!CreateDummySwapChain())
	{
		DestroyDummySwapChain();
		return false;
	}
	if (!hook_.Install(swapChain_, std::bind(&EngineGraphicsRuntime::RenderFrame, this)))
	{
		DestroyDummySwapChain();
		return false;
	}
	initialized_ = true;
	return true;
}

void EngineGraphicsRuntime::Shutdown() noexcept
{
	if (!initialized_ && swapChain_ == nullptr && window_ == nullptr)
	{
		return;
	}
	input_.Shutdown();
	hook_.Uninstall();
	DestroyDummySwapChain();
	initialized_ = false;
}

bool EngineGraphicsRuntime::CreateDummySwapChain() noexcept
{
	const HINSTANCE instance = GetModuleHandleW(nullptr);
	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.hInstance = instance;
	windowClass.lpfnWndProc = &DefWindowProcW;
	windowClass.lpszClassName = kWindowClassName;
	if (RegisterClassExW(&windowClass) == 0)
	{
		if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		{
			return false;
		}
	}
	windowClassRegistered_ = true;

	window_ = CreateWindowExW(
		0,
		kWindowClassName,
		L"DyingLightModEngine",
		WS_OVERLAPPED,
		0,
		0,
		2,
		2,
		nullptr,
		nullptr,
		instance,
		nullptr);
	if (window_ == nullptr)
	{
		return false;
	}

	DXGI_SWAP_CHAIN_DESC description{};
	description.BufferCount = 1;
	description.BufferDesc.Width = 2;
	description.BufferDesc.Height = 2;
	description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	description.OutputWindow = window_;
	description.SampleDesc.Count = 1;
	description.Windowed = TRUE;
	description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL featureLevel{};
	const HRESULT result = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&description,
		&swapChain_,
		nullptr,
		&featureLevel,
		nullptr);
	return SUCCEEDED(result) && swapChain_ != nullptr;
}

void EngineGraphicsRuntime::DestroyDummySwapChain() noexcept
{
	if (swapChain_ != nullptr)
	{
		swapChain_->Release();
		swapChain_ = nullptr;
	}
	if (window_ != nullptr)
	{
		DestroyWindow(window_);
		window_ = nullptr;
	}
	if (windowClassRegistered_)
	{
		UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
		windowClassRegistered_ = false;
	}
}

void EngineGraphicsRuntime::RenderFrame() noexcept
{
	void *swapChain = hook_.GetCurrentSwapChain();
	if (swapChain == nullptr)
	{
		return;
	}
	input_.AttachSwapChain(swapChain);
	EngineModManager::RenderFrame(swapChain);
	input_.RefreshUiState();
}
