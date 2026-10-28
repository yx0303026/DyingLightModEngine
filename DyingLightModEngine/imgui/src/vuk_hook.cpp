#include "dl_imgui/vuk_hook.h"

namespace dl::imgui {

	bool VukHook::Install(VulkanHook::QueuePresentFn queuePresent, RenderCallback callback) noexcept
	{
		return vulkanHook_.Install(queuePresent, std::move(callback));
	}

	void VukHook::Uninstall() noexcept
	{
		vulkanHook_.Uninstall();
	}

	bool VukHook::IsInstalled() const noexcept
	{
		return vulkanHook_.IsInstalled();
	}

} // namespace dl::imgui
