#pragma once

#include "sdk/EngineEntityAbi.h"

/* 玩家受伤：钩住 CHealthModule::AddDamageToBody 的两个入口，
   在伤害结算前改写/拦截，在结算后回报实际扣血量。 */
class EngineDamageRuntime final
{
public:
	static bool Install() noexcept;
	static bool IsInstalled() noexcept;
	static void Shutdown() noexcept;
	static DlModResult RegisterHook(
		ModEngineDamagePreHookFn pre,
		ModEngineDamagePostHookFn post) noexcept;
	static DlModResult UnregisterHook(
		ModEngineDamagePreHookFn pre,
		ModEngineDamagePostHookFn post) noexcept;
	/* 查询回调是否仍在注册表中，用于运行时重启后判断转发跳板是否需要重装。 */
	static bool IsHookRegistered(
		ModEngineDamagePreHookFn pre,
		ModEngineDamagePostHookFn post) noexcept;
};
