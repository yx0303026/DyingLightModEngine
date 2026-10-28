#pragma once

#include <functional>

namespace dl::imgui {

	using RenderCallback = std::function<void()>;

	class RenderHook {
	public:
		virtual ~RenderHook() = default;
		RenderHook(const RenderHook&) = delete;
		RenderHook& operator=(const RenderHook&) = delete;

		virtual void Uninstall() noexcept = 0;
		virtual bool IsInstalled() const noexcept = 0;

	protected:
		RenderHook() = default;
	};

} // namespace dl::imgui
