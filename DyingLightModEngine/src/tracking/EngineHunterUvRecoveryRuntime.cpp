#include "tracking/EngineHunterUvRecoveryRuntime.h"

#include "logging/EngineLogger.h"
#include "runtime/EngineEntityBindings.h"

#include <Windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>

extern "C"
{
	void *g_engine_entity_hunter_uv_recovery_start_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_recovery_complete_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_recovery_start_blocked_continuation = nullptr;
	void EngineEntityHunterUvRecoveryStartStub();
	void EngineEntityHunterUvRecoveryCompleteStub();
}

namespace
{
	constexpr std::uint32_t kUvHealAnticipation = 9;
	constexpr std::uint32_t kUvHeal = 10;
	constexpr std::uint32_t kUvHealRecovery = 11;
	constexpr std::size_t kStateWriteLength = 7;
	constexpr std::uint8_t kState9Bytes[] = {
		0xC7, 0x43, 0x38, 0x09, 0x00, 0x00, 0x00};
	constexpr std::uint8_t kState11Bytes[] = {
		0xC7, 0x47, 0x38, 0x0B, 0x00, 0x00, 0x00};

	SRWLOCK g_hookStateLock = SRWLOCK_INIT;
	DlModHunterUvRecoveryHookDescV1 g_hookState{};
	SRWLOCK g_stateLock = SRWLOCK_INIT;
	std::mutex g_installMutex;
	std::atomic_bool g_installed = false;

	struct RecoveryState
	{
		void *stateMachine = nullptr;
		bool inRecovery = false;
	};

	RecoveryState g_state{};

	const DlModHunterUvRecoveryHookDescV1 ReadHookState() noexcept
	{
		DlModHunterUvRecoveryHookDescV1 hooks{};
		AcquireSRWLockShared(&g_hookStateLock);
		hooks = g_hookState;
		ReleaseSRWLockShared(&g_hookStateLock);
		return hooks;
	}

	bool ReadState(void *stateMachine, std::uint32_t &state) noexcept
	{
		state = 0;
		if (stateMachine == nullptr)
			return false;
		__try
		{
			state = *reinterpret_cast<const std::uint32_t *>(
				static_cast<const std::uint8_t *>(stateMachine) +
				EngineEntityBindings::ZombieStateMachineStateOffset);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			state = 0;
			return false;
		}
	}

	bool InvokeStartHook(void *stateMachine, std::uint32_t newState) noexcept
	{
		const DlModHunterUvRecoveryHookDescV1 hooks = ReadHookState();
		if (hooks.onStart == nullptr)
			return true;
		__try
		{
			return hooks.onStart(stateMachine, newState, hooks.userData) != 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			EngineLogger::Logf(
				EngineLogLevel::Error,
				"UV 恢复开始回调异常，已放行原始状态写入（异常码 0x%08X）",
				GetExceptionCode());
			return true;
		}
	}

	void InvokeEndHook(void *stateMachine, std::uint32_t previousState) noexcept
	{
		const DlModHunterUvRecoveryHookDescV1 hooks = ReadHookState();
		if (hooks.onEnd == nullptr)
			return;
		__try
		{
			hooks.onEnd(stateMachine, previousState, hooks.userData);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			EngineLogger::Logf(
				EngineLogLevel::Error,
				"UV 恢复结束回调异常（异常码 0x%08X）",
				GetExceptionCode());
		}
	}

	bool ValidateSite(
		const std::uint8_t *site,
		const std::uint8_t *expected,
		std::size_t size) noexcept
	{
		return site != nullptr && std::memcmp(site, expected, size) == 0;
	}

	void RemoveHook(void *site) noexcept
	{
		if (site != nullptr)
		{
			MH_DisableHook(site);
			MH_RemoveHook(site);
		}
	}
}

extern "C" std::uint8_t EngineEntityHunterUvRecoveryStateCallback(
	void *stateMachine,
	std::uint32_t newState) noexcept
{
	if (stateMachine == nullptr)
		return 1;

	RecoveryState previous{};
	AcquireSRWLockShared(&g_stateLock);
	previous = g_state;
	ReleaseSRWLockShared(&g_stateLock);

	if (newState == kUvHealAnticipation)
	{
		std::uint32_t currentState = 0;
		if (!ReadState(stateMachine, currentState))
			return 1;
		const bool alreadyTracked =
			previous.inRecovery &&
			previous.stateMachine == stateMachine &&
			(currentState == kUvHealAnticipation);
		if (!alreadyTracked && !InvokeStartHook(stateMachine, newState))
		{
			EngineLogger::Info("UV 恢复被模组阻止。");
			return 0;
		}

		AcquireSRWLockExclusive(&g_stateLock);
		g_state.stateMachine = stateMachine;
		g_state.inRecovery = true;
		ReleaseSRWLockExclusive(&g_stateLock);
		return 1;
	}

	if (newState != kUvHealRecovery ||
		!previous.inRecovery ||
		previous.stateMachine != stateMachine)
	{
		return 1;
	}

	std::uint32_t currentState = 0;
	if (ReadState(stateMachine, currentState) &&
		(currentState == kUvHealAnticipation || currentState == kUvHeal))
	{
		InvokeEndHook(stateMachine, currentState);
	}
	AcquireSRWLockExclusive(&g_stateLock);
	g_state = {};
	ReleaseSRWLockExclusive(&g_stateLock);
	return 1;
}

bool EngineHunterUvRecoveryRuntime::Install() noexcept
{
#if !defined(_M_X64)
	return false;
#else
	const std::lock_guard<std::mutex> guard(g_installMutex);
	if (g_installed.load(std::memory_order_acquire))
		return true;

	const HMODULE game = EngineEntityBindings::GameModule();
	if (!EngineEntityBindings::IsSupportedGame(game))
		return false;

	auto *const base = reinterpret_cast<std::uint8_t *>(game);
	void *const startSite =
		base + EngineEntityBindings::HunterUvRecoveryStartSiteRva;
	void *const completeSite =
		base + EngineEntityBindings::HunterUvRecoveryCompleteSiteRva;
	if (!ValidateSite(
			static_cast<const std::uint8_t *>(startSite),
			kState9Bytes,
			sizeof(kState9Bytes)) ||
		!ValidateSite(
			static_cast<const std::uint8_t *>(completeSite),
			kState11Bytes,
			sizeof(kState11Bytes)))
	{
		EngineLogger::Warning("UV 恢复钩子站点与已验证游戏版本不匹配。");
		return false;
	}

	const MH_STATUS initStatus = MH_Initialize();
	if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
		return false;

	if (MH_CreateHook(
			startSite,
			reinterpret_cast<void *>(&EngineEntityHunterUvRecoveryStartStub),
			&g_engine_entity_hunter_uv_recovery_start_trampoline) != MH_OK)
	{
		return false;
	}
	if (MH_CreateHook(
			completeSite,
			reinterpret_cast<void *>(&EngineEntityHunterUvRecoveryCompleteStub),
			&g_engine_entity_hunter_uv_recovery_complete_trampoline) != MH_OK)
	{
		RemoveHook(startSite);
		g_engine_entity_hunter_uv_recovery_start_trampoline = nullptr;
		return false;
	}

	/* 阻止时从原始状态写入指令之后继续执行。 */
	g_engine_entity_hunter_uv_recovery_start_blocked_continuation =
		static_cast<std::uint8_t *>(startSite) + kStateWriteLength;

	if (MH_EnableHook(startSite) != MH_OK ||
		MH_EnableHook(completeSite) != MH_OK)
	{
		RemoveHook(startSite);
		RemoveHook(completeSite);
		g_engine_entity_hunter_uv_recovery_start_trampoline = nullptr;
		g_engine_entity_hunter_uv_recovery_complete_trampoline = nullptr;
		g_engine_entity_hunter_uv_recovery_start_blocked_continuation = nullptr;
		return false;
	}

	g_installed.store(true, std::memory_order_release);
	EngineLogger::Info("UV 恢复追踪已安装。");
	return true;
#endif
}

bool EngineHunterUvRecoveryRuntime::IsInstalled() noexcept
{
	return g_installed.load(std::memory_order_acquire);
}

void EngineHunterUvRecoveryRuntime::Shutdown() noexcept
{
#if defined(_M_X64)
	const std::lock_guard<std::mutex> guard(g_installMutex);
	if (g_installed.exchange(false, std::memory_order_acq_rel))
	{
		const HMODULE game = EngineEntityBindings::GameModule();
		if (game != nullptr)
		{
			auto *const base = reinterpret_cast<std::uint8_t *>(game);
			RemoveHook(base + EngineEntityBindings::HunterUvRecoveryStartSiteRva);
			RemoveHook(
				base + EngineEntityBindings::HunterUvRecoveryCompleteSiteRva);
		}
	}

	g_engine_entity_hunter_uv_recovery_start_trampoline = nullptr;
	g_engine_entity_hunter_uv_recovery_complete_trampoline = nullptr;
	g_engine_entity_hunter_uv_recovery_start_blocked_continuation = nullptr;
	AcquireSRWLockExclusive(&g_hookStateLock);
	g_hookState = {};
	ReleaseSRWLockExclusive(&g_hookStateLock);
	AcquireSRWLockExclusive(&g_stateLock);
	g_state = {};
	ReleaseSRWLockExclusive(&g_stateLock);
#endif
}

DlModResult EngineHunterUvRecoveryRuntime::RegisterHooks(
	const DlModHunterUvRecoveryHookDescV1 &desc) noexcept
{
	if (desc.structSize < sizeof(DlModHunterUvRecoveryHookDescV1) ||
		desc.abiVersion != MODSDK_ABI_VERSION ||
		(desc.onStart == nullptr && desc.onEnd == nullptr))
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (!g_installed.load(std::memory_order_acquire))
		return DLMOD_RESULT_INVALID_STATE;

	AcquireSRWLockExclusive(&g_hookStateLock);
	if (g_hookState.onStart != nullptr || g_hookState.onEnd != nullptr)
	{
		ReleaseSRWLockExclusive(&g_hookStateLock);
		return DLMOD_RESULT_INVALID_STATE;
	}
	g_hookState = desc;
	ReleaseSRWLockExclusive(&g_hookStateLock);
	return DLMOD_RESULT_SUCCESS;
}

DlModResult EngineHunterUvRecoveryRuntime::UnregisterHooks() noexcept
{
	AcquireSRWLockExclusive(&g_hookStateLock);
	g_hookState = {};
	ReleaseSRWLockExclusive(&g_hookStateLock);
	return DLMOD_RESULT_SUCCESS;
}
