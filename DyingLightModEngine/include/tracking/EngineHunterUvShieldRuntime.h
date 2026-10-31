#pragma once

#include "sdk/EngineEntityAbi.h"

/* 猎手紫外线护盾：钩住 BTZ 僵尸状态机的护盾计时写入点。 */
class EngineHunterUvShieldRuntime final
{
public:
	static bool Install() noexcept;
	static bool IsInstalled() noexcept;
	static void Shutdown() noexcept;
	static bool Read(DlModHunterUvShieldV1 &snapshot) noexcept;
	static DlModResult SetOverride(uint32_t mode, float value) noexcept;
	static DlModResult SetChargeOverride(int32_t charges) noexcept;
	static DlModResult SetDurationOverride(float seconds) noexcept;
	static DlModResult SetProgressRatio(float ratio) noexcept;
	static DlModResult RegisterHooks(
		const DlModHunterUvShieldHookDescV1 &desc) noexcept;
	static DlModResult UnregisterHooks() noexcept;
};
