#pragma once

#include <imgui.h>

namespace dl::imgui {

	enum class StylePreset {
		Sidebar
	};

	// Applies a complete preset to the currently active ImGui context.
	void ApplyStyle(StylePreset preset = StylePreset::Sidebar) noexcept;

	// Applies the engine's default sidebar-oriented dark theme.
	void ApplyDefaultStyle() noexcept;

} // namespace dl::imgui
