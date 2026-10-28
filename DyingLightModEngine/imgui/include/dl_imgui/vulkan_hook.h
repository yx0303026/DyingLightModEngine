#pragma once

#include "dl_imgui/render_hook.h"

#include <cstdint>

namespace dl::imgui {

	using VkQueueHandle = void*;
	using VkPresentInfo = void;
	using VkResult = std::int32_t;

	class VulkanHook final : public RenderHook {
	public:
		using QueuePresentFn = VkResult(__stdcall*)(VkQueueHandle queue, const VkPresentInfo* presentInfo);

		VulkanHook() = default;
		~VulkanHook() override;
		VulkanHook(const VulkanHook&) = delete;
		VulkanHook& operator=(const VulkanHook&) = delete;

		bool Install(QueuePresentFn target, RenderCallback callback) noexcept;
		void Uninstall() noexcept override;
		bool IsInstalled() const noexcept override;

	private:
		static VkResult __stdcall QueuePresentDetour(VkQueueHandle queue, const VkPresentInfo* presentInfo);
		static VulkanHook* ActiveHook() noexcept;

		QueuePresentFn originalQueuePresent_ = nullptr;
		QueuePresentFn targetQueuePresent_ = nullptr;
		RenderCallback callback_;
		bool installed_ = false;
};

} // namespace dl::imgui
