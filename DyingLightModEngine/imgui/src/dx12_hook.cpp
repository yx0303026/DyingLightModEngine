#include "dl_imgui/dx12_hook.h"

#include "dl_imgui/hook_runtime.h"

#include <MinHook.h>

namespace dl::imgui {
namespace {
	Dx12Hook* g_activeHook = nullptr;
}

	Dx12Hook::~Dx12Hook()
	{
		Uninstall();
	}

	bool Dx12Hook::Install(void* swapChain, RenderCallback callback) noexcept
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

	void Dx12Hook::Uninstall() noexcept
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
		originalPresent_ = nullptr;
		swapChain_ = nullptr;
		target_ = nullptr;
		installed_ = false;
	}

	bool Dx12Hook::IsInstalled() const noexcept
	{
		return installed_;
	}

	long __stdcall Dx12Hook::PresentDetour(void* swapChain, unsigned int syncInterval, unsigned int flags)
	{
		Dx12Hook* hook = ActiveHook();
		if (hook != nullptr && hook->callback_) {
			try {
				hook->callback_();
			}
			catch (...) {
			}
		}
		return hook != nullptr && hook->originalPresent_ != nullptr
			? hook->originalPresent_(swapChain, syncInterval, flags)
			: 0L;
	}

	Dx12Hook* Dx12Hook::ActiveHook() noexcept
	{
		return g_activeHook;
	}

} // namespace dl::imgui
