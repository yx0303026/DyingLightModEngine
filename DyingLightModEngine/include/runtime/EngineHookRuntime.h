#pragma once

#include "sdk/EngineAbi.h"

#include <string>

/* Hook 运行时概览：集中管理 MinHook 目标，并维护可卸载的模组 detour 链。 */
class EngineHookRuntime final {
public:
	static const DlModHookApiV1* GetApi() noexcept;
	static DlModResult RegisterForMod(
		const std::string& modId,
		void* target,
		void* detour,
		void** original) noexcept;
	static bool RemoveForMod(const std::string& modId) noexcept;
	static void RemoveAll() noexcept;

private:
	static DlModResult MODSDK_CALL RegisterHook(
		void* target,
		void* detour,
		void** original) noexcept;
};

namespace EngineHookRuntimeDetail {
	const std::string* SetLoadingMod(const std::string* modId) noexcept;
}
