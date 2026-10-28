#pragma once

#include <Windows.h>

#include <cstddef>

namespace dl::imgui::detail {

	bool EnsureHookRuntime() noexcept;
	void* GetVtableFunction(void* object, std::size_t index) noexcept;

} // namespace dl::imgui::detail
