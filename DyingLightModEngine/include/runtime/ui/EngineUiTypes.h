#pragma once

#include <cstdint>

namespace dl::engineui {

/* UI 类型概览：定义 UI 后端、可见性和通用句柄。 */
enum class UiMode : std::uint32_t {
	Native = 0,
	Pak = 1
};

enum class UiVisibility : std::uint32_t {
	Hidden = 0,
	Visible = 1
};

using UiHandle = std::uintptr_t;

struct UiVector3 {
	float x = 0.0F;
	float y = 0.0F;
	float z = 0.0F;
};

struct UiColor {
	float red = 1.0F;
	float green = 1.0F;
	float blue = 1.0F;
	float alpha = 1.0F;
};

struct UiElementOptions {
	UiVector3 position{};
	UiVector3 size{};
	UiColor color{};
	UiVisibility visibility = UiVisibility::Visible;
};

} // namespace dl::engineui
