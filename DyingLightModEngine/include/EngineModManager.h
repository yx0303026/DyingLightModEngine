#pragma once

#include "EngineAbi.h"

#ifdef __cplusplus

/*
 * EngineModManager loads modules from either:
 *
 *   <modsDirectory>/<modName>/<mod_name>.dll
 *   <modsDirectory>/<modName>.dll
 *
 * For the directory layout, <mod_name> is the snake_case form of <modName>;
 * the manager falls back to <modName>.dll when the snake_case file is absent.
 * Passing nullptr for modsDirectory uses the directory containing the host
 * executable and its Mods subdirectory.
 */
class EngineModManager final {
public:
	static MODSDK_API DlModResult LoadAll(const char* modsDirectory = nullptr) noexcept;
	static MODSDK_API DlModResult Load(
		const char* modName,
		const char* modsDirectory = nullptr) noexcept;
	static MODSDK_API DlModResult Unload(const char* modId) noexcept;
	static MODSDK_API DlModResult UnloadAll() noexcept;
	static MODSDK_API uint32_t GetLoadedCount() noexcept;
	static MODSDK_API void RenderFrame(void* graphicsContext) noexcept;
	static MODSDK_API bool IsUiOpen() noexcept;
	static MODSDK_API void OnWindowMessage(
		uintptr_t window,
		uint32_t message,
		uintptr_t wParam,
		intptr_t lParam) noexcept;
};

#endif
