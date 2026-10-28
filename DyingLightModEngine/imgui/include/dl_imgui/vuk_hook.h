#pragma once

#include "dl_imgui/vulkan_hook.h"

namespace dl::imgui {

	class VukHook final : public RenderHook {
	public:
		VukHook() = default;
		~VukHook() override = default;
		VukHook(const VukHook&) = delete;
		VukHook& operator=(const VukHook&) = delete;

		bool Install(VulkanHook::QueuePresentFn queuePresent, RenderCallback callback) noexcept;
		void Uninstall() noexcept override;
		bool IsInstalled() const noexcept override;

	private:
		VulkanHook vulkanHook_;
};

} // namespace dl::imgui
