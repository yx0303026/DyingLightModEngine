#include "dl_imgui/hook_runtime.h"

#include <MinHook.h>

#include <cstddef>

namespace dl::imgui::detail {

	bool EnsureHookRuntime() noexcept
	{
		static const MH_STATUS status = MH_Initialize();
		static const bool initialized = status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
		return initialized;
	}

	void* GetVtableFunction(void* object, std::size_t index) noexcept
	{
		if (object == nullptr) {
			return nullptr;
		}
		void** vtable = *static_cast<void***>(object);
		return vtable == nullptr ? nullptr : vtable[index];
	}

} // namespace dl::imgui::detail
