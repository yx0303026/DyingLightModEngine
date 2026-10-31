#pragma once

#include "sdk/EngineEntityAbi.h"

/* 猎手紫外线恢复：阻止状态 9 的写入，并在状态 11 写入点通知本轮恢复完成。 */
class EngineHunterUvRecoveryRuntime final
{
public:
	static bool Install() noexcept;
	static bool IsInstalled() noexcept;
	static void Shutdown() noexcept;
	static DlModResult RegisterHooks(
		const DlModHunterUvRecoveryHookDescV1 &desc) noexcept;
	static DlModResult UnregisterHooks() noexcept;
};
