#pragma once

#include "dl_imgui/render_hook.h"

namespace dl::imgui {

	class Dx12Hook final : public RenderHook {
	public:
		using PresentFn = long(__stdcall*)(void* swapChain, unsigned int syncInterval, unsigned int flags);

		Dx12Hook() = default;
		~Dx12Hook() override;
		Dx12Hook(const Dx12Hook&) = delete;
		Dx12Hook& operator=(const Dx12Hook&) = delete;

		bool Install(void* swapChain, RenderCallback callback) noexcept;
		void Uninstall() noexcept override;
		bool IsInstalled() const noexcept override;

	private:
		static long __stdcall PresentDetour(void* swapChain, unsigned int syncInterval, unsigned int flags);
		static Dx12Hook* ActiveHook() noexcept;

		void* swapChain_ = nullptr;
		void* target_ = nullptr;
		PresentFn originalPresent_ = nullptr;
		RenderCallback callback_;
		bool installed_ = false;
};

} // namespace dl::imgui
