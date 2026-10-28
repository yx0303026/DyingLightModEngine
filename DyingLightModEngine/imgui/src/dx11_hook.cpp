#include "dl_imgui/dx11_hook.h"

#include "hook_runtime.h"

#include <MinHook.h>

namespace dl::imgui {
namespace {
	Dx11Hook* g_activeHook = nullptr;
}

	Dx11Hook::~Dx11Hook()
	{
		Uninstall();
	}

	bool Dx11Hook::Install(void* swapChain, RenderCallback callback) noexcept
	{
		if (installed_ || swapChain == nullptr || !callback || !detail::EnsureHookRuntime()) {
			return false;
		}
		void* target = detail::GetVtableFunction(swapChain, 8U);
		if (target == nullptr || MH_CreateHook(target, &PresentDetour, reinterpret_cast<void**>(&originalPresent_)) != MH_OK) {
			MH_RemoveHook(target);
			return false;
		}
		swapChain_ = swapChain;
		target_ = target;
		try {
			callback_ = std::move(callback);
		}
		catch (...) {
			MH_RemoveHook(target);
			originalPresent_ = nullptr;
			swapChain_ = nullptr;
			target_ = nullptr;
			return false;
		}
		g_activeHook = this;
		if (MH_EnableHook(target) != MH_OK) {
			g_activeHook = nullptr;
			callback_ = {};
			MH_RemoveHook(target);
			originalPresent_ = nullptr;
			swapChain_ = nullptr;
			target_ = nullptr;
			return false;
		}
		installed_ = true;
		return true;
	}

	void Dx11Hook::Uninstall() noexcept
	{
		if (!installed_) {
			return;
		}
		if (target_ != nullptr) {
			MH_DisableHook(target_);
			MH_RemoveHook(target_);
		}
		if (g_activeHook == this) {
			g_activeHook = nullptr;
		}
		callback_ = {};
		currentSwapChain_ = nullptr;
		originalPresent_ = nullptr;
		swapChain_ = nullptr;
		target_ = nullptr;
		installed_ = false;
	}

	bool Dx11Hook::IsInstalled() const noexcept
	{
		return installed_;
	}

	void* Dx11Hook::GetCurrentSwapChain() const noexcept
	{
		return currentSwapChain_;
	}

	long __stdcall Dx11Hook::PresentDetour(void* swapChain, unsigned int syncInterval, unsigned int flags)
	{
		Dx11Hook* hook = ActiveHook();
		if (hook != nullptr) {
			hook->currentSwapChain_ = swapChain;
		}
		if (hook != nullptr && hook->callback_) {
			try {
				hook->callback_();
			}
			catch (...) {
			}
		}
		if (hook != nullptr) {
			hook->currentSwapChain_ = nullptr;
		}
		return hook != nullptr && hook->originalPresent_ != nullptr
			? hook->originalPresent_(swapChain, syncInterval, flags)
			: 0L;
	}

	Dx11Hook* Dx11Hook::ActiveHook() noexcept
	{
		return g_activeHook;
	}

} // namespace dl::imgui
