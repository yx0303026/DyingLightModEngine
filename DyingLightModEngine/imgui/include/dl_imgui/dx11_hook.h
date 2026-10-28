#pragma once

#include "dl_imgui/render_hook.h"

namespace dl::imgui {

	class Dx11Hook final : public RenderHook {
	public:
		using PresentFn = long(__stdcall*)(void* swapChain, unsigned int syncInterval, unsigned int flags);

		Dx11Hook() = default;
		~Dx11Hook() override;
		Dx11Hook(const Dx11Hook&) = delete;
		Dx11Hook& operator=(const Dx11Hook&) = delete;

		bool Install(void* swapChain, RenderCallback callback) noexcept;
		void Uninstall() noexcept override;
		bool IsInstalled() const noexcept override;
		void* GetCurrentSwapChain() const noexcept;

	private:
		static long __stdcall PresentDetour(void* swapChain, unsigned int syncInterval, unsigned int flags);
		static Dx11Hook* ActiveHook() noexcept;

		void* swapChain_ = nullptr;
		void* target_ = nullptr;
		PresentFn originalPresent_ = nullptr;
		RenderCallback callback_;
		void* currentSwapChain_ = nullptr;
		bool installed_ = false;
};

} // namespace dl::imgui
