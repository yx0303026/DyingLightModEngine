#pragma once

#include "dl_imgui/render_hook.h"

#include <Windows.h>

namespace dl::imgui {

	class OpenGlHook final : public RenderHook {
	public:
		using SwapBuffersFn = int(__stdcall*)(HDC deviceContext);

		OpenGlHook() = default;
		~OpenGlHook() override;
		OpenGlHook(const OpenGlHook&) = delete;
		OpenGlHook& operator=(const OpenGlHook&) = delete;

		bool Install(RenderCallback callback) noexcept;
		bool Install(SwapBuffersFn target, RenderCallback callback) noexcept;
		void Uninstall() noexcept override;
		bool IsInstalled() const noexcept override;

	private:
		static int __stdcall SwapBuffersDetour(HDC deviceContext);
		static OpenGlHook* ActiveHook() noexcept;

		SwapBuffersFn originalSwapBuffers_ = nullptr;
		SwapBuffersFn targetSwapBuffers_ = nullptr;
		RenderCallback callback_;
		bool installed_ = false;
};

} // namespace dl::imgui
