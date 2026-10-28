#include "dl_imgui/opengl_hook.h"

#include "hook_runtime.h"

#include <MinHook.h>

namespace dl::imgui {
namespace {
	OpenGlHook* g_activeHook = nullptr;
}

	OpenGlHook::~OpenGlHook()
	{
		Uninstall();
	}

	bool OpenGlHook::Install(RenderCallback callback) noexcept
	{
		HMODULE module = GetModuleHandleA("gdi32.dll");
		if (module == nullptr) {
			return false;
		}
		return Install(reinterpret_cast<SwapBuffersFn>(GetProcAddress(module, "SwapBuffers")), std::move(callback));
	}

	bool OpenGlHook::Install(SwapBuffersFn target, RenderCallback callback) noexcept
	{
		if (installed_ || target == nullptr || !callback || !detail::EnsureHookRuntime()) {
			return false;
		}
		if (MH_CreateHook(target, &SwapBuffersDetour, reinterpret_cast<void**>(&originalSwapBuffers_)) != MH_OK) {
			MH_RemoveHook(target);
			return false;
		}
		targetSwapBuffers_ = target;
		try {
			callback_ = std::move(callback);
		}
		catch (...) {
			MH_RemoveHook(target);
			originalSwapBuffers_ = nullptr;
			targetSwapBuffers_ = nullptr;
			return false;
		}
		g_activeHook = this;
		if (MH_EnableHook(target) != MH_OK) {
			g_activeHook = nullptr;
			callback_ = {};
			MH_RemoveHook(target);
			originalSwapBuffers_ = nullptr;
			targetSwapBuffers_ = nullptr;
			return false;
		}
		installed_ = true;
		return true;
	}

	void OpenGlHook::Uninstall() noexcept
	{
		if (!installed_) {
			return;
		}
		if (targetSwapBuffers_ != nullptr) {
			MH_DisableHook(targetSwapBuffers_);
			MH_RemoveHook(targetSwapBuffers_);
		}
		if (g_activeHook == this) {
			g_activeHook = nullptr;
		}
		callback_ = {};
		originalSwapBuffers_ = nullptr;
		targetSwapBuffers_ = nullptr;
		installed_ = false;
	}

	bool OpenGlHook::IsInstalled() const noexcept
	{
		return installed_;
	}

	int __stdcall OpenGlHook::SwapBuffersDetour(HDC deviceContext)
	{
		OpenGlHook* hook = ActiveHook();
		if (hook != nullptr && hook->callback_) {
			try {
				hook->callback_();
			}
			catch (...) {
			}
		}
		return hook != nullptr && hook->originalSwapBuffers_ != nullptr ? hook->originalSwapBuffers_(deviceContext) : 0;
	}

	OpenGlHook* OpenGlHook::ActiveHook() noexcept
	{
		return g_activeHook;
	}

} // namespace dl::imgui
