#pragma once

#include "EngineAbi.h"

#ifdef __cplusplus

/*
 * EngineModManager loads modules from:
 *
 *   <modsDirectory>/<modName>/<mod_name>.dll
 *
 * where <mod_name> is the snake_case form of <modName>. If that file is not
 * present, the manager also accepts <modName>.dll in the same directory.
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
};

#endif
