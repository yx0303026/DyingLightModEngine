#include "dl_imgui/vulkan_hook.h"

#include "hook_runtime.h"

#include <MinHook.h>

namespace dl::imgui {
namespace {
	VulkanHook* g_activeHook = nullptr;
}

	VulkanHook::~VulkanHook()
	{
		Uninstall();
	}

	bool VulkanHook::Install(QueuePresentFn target, RenderCallback callback) noexcept
	{
		if (installed_ || target == nullptr || !callback || !detail::EnsureHookRuntime()) {
			return false;
		}
		if (MH_CreateHook(target, &QueuePresentDetour, reinterpret_cast<void**>(&originalQueuePresent_)) != MH_OK) {
			MH_RemoveHook(target);
			return false;
		}
		targetQueuePresent_ = target;
		try {
			callback_ = std::move(callback);
		}
		catch (...) {
			MH_RemoveHook(target);
			originalQueuePresent_ = nullptr;
			targetQueuePresent_ = nullptr;
			return false;
		}
		g_activeHook = this;
		if (MH_EnableHook(target) != MH_OK) {
			g_activeHook = nullptr;
			callback_ = {};
			MH_RemoveHook(target);
			originalQueuePresent_ = nullptr;
			targetQueuePresent_ = nullptr;
			return false;
		}
		installed_ = true;
		return true;
	}

	void VulkanHook::Uninstall() noexcept
	{
		if (!installed_) {
			return;
		}
		if (targetQueuePresent_ != nullptr) {
			MH_DisableHook(targetQueuePresent_);
			MH_RemoveHook(targetQueuePresent_);
		}
		if (g_activeHook == this) {
			g_activeHook = nullptr;
		}
		callback_ = {};
		originalQueuePresent_ = nullptr;
		targetQueuePresent_ = nullptr;
		installed_ = false;
	}

	bool VulkanHook::IsInstalled() const noexcept
	{
		return installed_;
	}

	VkResult __stdcall VulkanHook::QueuePresentDetour(VkQueueHandle queue, const VkPresentInfo* presentInfo)
	{
		VulkanHook* hook = ActiveHook();
		if (hook != nullptr && hook->callback_) {
			try {
				hook->callback_();
			}
			catch (...) {
			}
		}
		return hook != nullptr && hook->originalQueuePresent_ != nullptr
			? hook->originalQueuePresent_(queue, presentInfo)
			: static_cast<VkResult>(-4);
	}

	VulkanHook* VulkanHook::ActiveHook() noexcept
	{
		return g_activeHook;
	}

} // namespace dl::imgui
