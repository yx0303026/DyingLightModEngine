#include "tracking/EngineHunterUvShieldRuntime.h"

#include "logging/EngineLogger.h"
#include "runtime/EngineEntityBindings.h"

#include <Windows.h>
#include <MinHook.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>

extern "C"
{
	void *g_engine_entity_hunter_uv_shield_timer_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_shield_client_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_shield_smoke_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_shield_ground_pound_trampoline = nullptr;
	void EngineEntityHunterUvShieldTimerStub();
	void EngineEntityHunterUvShieldClientStub();
	void EngineEntityHunterUvShieldSmokeStub();
	void EngineEntityHunterUvShieldGroundPoundStub();
}

namespace
{
	/* 四处写入点都是 movss [rbx+0A4h], xmm?，用后续指令区分所属路径。 */
	constexpr std::uint8_t kTimerSignature[] = {
		0xF3, 0x0F, 0x11, 0x83, 0xA4, 0x00, 0x00, 0x00,
		0x48, 0x8B, 0xBC, 0x24, 0x98, 0x00, 0x00, 0x00};
	constexpr std::uint8_t kClientSignature[] = {
		0xF3, 0x0F, 0x11, 0x8B, 0xA4, 0x00, 0x00, 0x00,
		0x0F, 0x28, 0x74, 0x24, 0x20, 0x48, 0x83, 0xC4};
	constexpr std::uint8_t kSmokeSignature[] = {
		0xF3, 0x0F, 0x11, 0x83, 0xA4, 0x00, 0x00, 0x00,
		0xC6, 0x83, 0xA8, 0x00, 0x00, 0x00, 0x01};
	constexpr std::uint8_t kGroundPoundSignature[] = {
		0xF3, 0x0F, 0x11, 0x83, 0xA4, 0x00, 0x00, 0x00,
		0x40, 0x88, 0xB3, 0xA8, 0x00, 0x00, 0x00};
	constexpr float kMaximumShieldSeconds = 3600.0F;
	constexpr float kMaximumScale = 100.0F;
	constexpr std::int32_t kMaximumCharges = 99;

	struct ShieldState
	{
		void *stateMachine = nullptr;
		float durationSeconds = 0.0F;
		double observedTimeSeconds = 0.0;
	};

	SRWLOCK g_stateLock = SRWLOCK_INIT;
	ShieldState g_state{};
	std::atomic<std::uint32_t> g_mode{DLMOD_HUNTER_UV_SHIELD_MODE_GAME};
	std::atomic<float> g_overrideValue{0.0F};
	std::atomic<std::int32_t> g_chargeOverride{-1};
	std::atomic<float> g_durationOverride{0.0F};
	std::atomic_bool g_installed = false;
	std::mutex g_installMutex;
	LARGE_INTEGER g_counterFrequency{};
	INIT_ONCE g_counterFrequencyOnce = INIT_ONCE_STATIC_INIT;

	BOOL CALLBACK InitializeCounterFrequency(PINIT_ONCE, PVOID, PVOID *)
	{
		return QueryPerformanceFrequency(&g_counterFrequency) &&
			   g_counterFrequency.QuadPart > 0;
	}

	bool ReadTimeSeconds(double &seconds) noexcept
	{
		seconds = 0.0;
		if (!InitOnceExecuteOnce(
				&g_counterFrequencyOnce, InitializeCounterFrequency, nullptr, nullptr))
		{
			return false;
		}
		LARGE_INTEGER counter{};
		if (!QueryPerformanceCounter(&counter))
			return false;
		seconds = static_cast<double>(counter.QuadPart) /
				  static_cast<double>(g_counterFrequency.QuadPart);
		return std::isfinite(seconds) && seconds > 0.0;
	}

	template <typename Value>
	bool ReadField(const void *object, std::uintptr_t offset, Value &value) noexcept
	{
		value = {};
		if (object == nullptr)
			return false;
		__try
		{
			value = *reinterpret_cast<const Value *>(
				static_cast<const std::uint8_t *>(object) + offset);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			value = {};
			return false;
		}
	}

	float SanitizeShieldSeconds(float value) noexcept
	{
		if (!std::isfinite(value) || value < 0.0F)
			return 0.0F;
		return (std::min)(value, kMaximumShieldSeconds);
	}

	void RecordObservation(
		void *stateMachine,
		float durationSeconds,
		bool activation) noexcept
	{
		if (stateMachine == nullptr)
			return;
		double now = 0.0;
		ReadTimeSeconds(now);
		AcquireSRWLockExclusive(&g_stateLock);
		if (g_state.stateMachine != stateMachine)
		{
			g_state = {};
			g_state.stateMachine = stateMachine;
		}
		if (activation)
			g_state.durationSeconds = SanitizeShieldSeconds(durationSeconds);
		g_state.observedTimeSeconds = now;
		ReleaseSRWLockExclusive(&g_stateLock);
	}

	/* PIN 模式下同步点亮 HUD 的紫外格挡标记，避免护盾生效但不显示。 */
	void RaiseShieldFlag(void *stateMachine) noexcept
	{
		if (stateMachine == nullptr)
			return;
		__try
		{
			*reinterpret_cast<std::uint8_t *>(
				static_cast<std::uint8_t *>(stateMachine) +
				EngineEntityBindings::ZombieStateMachineShieldFlagOffset) = 1;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	/* 充能覆盖：直接写状态机剩余充能字段，游戏消耗后下一帧即被拉回设定值。 */
	void PinCharges(void* stateMachine) noexcept
	{
		if (stateMachine == nullptr)
			return;
		const std::int32_t charges = g_chargeOverride.load(std::memory_order_acquire);
		if (charges < 0)
			return;
		__try
		{
			*reinterpret_cast<std::int32_t*>(
				static_cast<std::uint8_t*>(stateMachine) +
				EngineEntityBindings::ZombieStateMachineSmokeChargesOffset) = charges;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	float ApplyTimerOverride(float value) noexcept
	{
		const std::uint32_t mode = g_mode.load(std::memory_order_acquire);
		const float setting = g_overrideValue.load(std::memory_order_acquire);
		switch (mode)
		{
		case DLMOD_HUNTER_UV_SHIELD_MODE_PIN:
			return std::isfinite(setting) && setting > value ? setting : value;
		case DLMOD_HUNTER_UV_SHIELD_MODE_OFF:
			return 0.0F;
		case DLMOD_HUNTER_UV_SHIELD_MODE_SCALE:
			return std::isfinite(setting) && setting >= 0.0F
					   ? SanitizeShieldSeconds(value * setting)
					   : value;
		default:
			return value;
		}
	}

	float ApplyDurationOverride(float duration) noexcept
	{
		const float forced = g_durationOverride.load(std::memory_order_acquire);
		if (std::isfinite(forced) && forced > 0.0F)
			return SanitizeShieldSeconds(forced);
		if (g_mode.load(std::memory_order_acquire) != DLMOD_HUNTER_UV_SHIELD_MODE_SCALE)
			return duration;
		const float setting = g_overrideValue.load(std::memory_order_acquire);
		return std::isfinite(setting) && setting >= 0.0F
				   ? SanitizeShieldSeconds(duration * setting)
				   : duration;
	}

	float BitsToFloat(std::uint32_t bits) noexcept
	{
		float value = 0.0F;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	bool CreateHook(
		void *target,
		void *detour,
		void **trampoline,
		const char *name) noexcept
	{
		const MH_STATUS status = MH_CreateHook(target, detour, trampoline);
		if (status != MH_OK)
		{
			EngineLogger::Logf(
				EngineLogLevel::Warning,
				"Hunter UV shield hook %s failed to create (%d)",
				name,
				static_cast<int>(status));
			return false;
		}
		return true;
	}

	void RemoveHooks(
		void *updateSite,
		void *clientSite,
		void *smokeSite,
		void *groundPoundSite) noexcept
	{
		for (void *site : {updateSite, clientSite, smokeSite, groundPoundSite})
		{
			if (site != nullptr)
			{
				MH_DisableHook(site);
				MH_RemoveHook(site);
			}
		}
	}
}

extern "C" float EngineEntity_OnHunterUvShieldTimer(
	void *stateMachine,
	std::uint32_t valueBits) noexcept
{
	const float value = BitsToFloat(valueBits);
	if (stateMachine == nullptr || !std::isfinite(value))
		return value;
	RecordObservation(stateMachine, value, false);
	PinCharges(stateMachine);
	const float result = ApplyTimerOverride(value);
	if (result > 0.0F &&
		g_mode.load(std::memory_order_relaxed) == DLMOD_HUNTER_UV_SHIELD_MODE_PIN)
	{
		RaiseShieldFlag(stateMachine);
	}
	return result;
}

extern "C" float EngineEntity_OnHunterUvShieldClientTimer(
	void *stateMachine,
	std::uint32_t valueBits) noexcept
{
	const float value = BitsToFloat(valueBits);
	if (stateMachine == nullptr || !std::isfinite(value))
		return value;
	RecordObservation(stateMachine, value, false);
	PinCharges(stateMachine);
	/* 倍率只作用于主更新路径，避免同一帧被两条路径各缩放一次。 */
	const std::uint32_t mode = g_mode.load(std::memory_order_acquire);
	if (mode == DLMOD_HUNTER_UV_SHIELD_MODE_OFF)
		return 0.0F;
	if (mode == DLMOD_HUNTER_UV_SHIELD_MODE_PIN)
	{
		const float setting = g_overrideValue.load(std::memory_order_acquire);
		if (std::isfinite(setting) && setting > value)
			return setting;
	}
	return value;
}

extern "C" float EngineEntity_OnHunterUvShieldDefensiveSmoke(
	void *stateMachine,
	std::uint32_t durationBits) noexcept
{
	const float duration = BitsToFloat(durationBits);
	if (stateMachine == nullptr || !std::isfinite(duration))
		return duration;
	RecordObservation(stateMachine, duration, true);
	return ApplyDurationOverride(duration);
}

extern "C" float EngineEntity_OnHunterUvShieldGroundPound(
	void *stateMachine,
	std::uint32_t durationBits) noexcept
{
	const float duration = BitsToFloat(durationBits);
	if (stateMachine == nullptr || !std::isfinite(duration))
		return duration;
	RecordObservation(stateMachine, duration, true);
	return ApplyDurationOverride(duration);
}

bool EngineHunterUvShieldRuntime::Install() noexcept
{
#if !defined(_M_X64)
	return false;
#else
	if (g_installed.load(std::memory_order_acquire))
		return true;
	const std::lock_guard<std::mutex> guard(g_installMutex);
	if (g_installed.load(std::memory_order_relaxed))
		return true;
	const HMODULE game = EngineEntityBindings::GameModule();
	if (!EngineEntityBindings::IsSupportedGame(game))
		return false;

	void *updateSite = reinterpret_cast<std::uint8_t *>(game) +
					   EngineEntityBindings::HunterUvShieldUpdateSiteRva;
	void *clientSite = reinterpret_cast<std::uint8_t *>(game) +
					   EngineEntityBindings::HunterUvShieldClientSiteRva;
	void *smokeSite = reinterpret_cast<std::uint8_t *>(game) +
					  EngineEntityBindings::HunterUvShieldSmokeSiteRva;
	void *groundPoundSite = reinterpret_cast<std::uint8_t *>(game) +
							EngineEntityBindings::HunterUvShieldGroundPoundSiteRva;
	if (std::memcmp(
			updateSite, kTimerSignature, sizeof(kTimerSignature)) != 0 ||
		std::memcmp(
			clientSite, kClientSignature, sizeof(kClientSignature)) != 0 ||
		std::memcmp(
			smokeSite, kSmokeSignature, sizeof(kSmokeSignature)) != 0 ||
		std::memcmp(
			groundPoundSite,
			kGroundPoundSignature,
			sizeof(kGroundPoundSignature)) != 0)
	{
		EngineLogger::Warning(
			"Hunter UV shield hook sites did not match the expected game build");
		return false;
	}
	const MH_STATUS initStatus = MH_Initialize();
	if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
		return false;

	void *timerTrampoline = nullptr;
	void *clientTrampoline = nullptr;
	void *smokeTrampoline = nullptr;
	void *groundPoundTrampoline = nullptr;
	if (!CreateHook(
			updateSite,
			reinterpret_cast<void *>(&EngineEntityHunterUvShieldTimerStub),
			&timerTrampoline,
			"timer"))
	{
		return false;
	}
	if (!CreateHook(
			clientSite,
			reinterpret_cast<void *>(&EngineEntityHunterUvShieldClientStub),
			&clientTrampoline,
			"client timer"))
	{
		MH_RemoveHook(updateSite);
		return false;
	}
	if (!CreateHook(
			smokeSite,
			reinterpret_cast<void *>(&EngineEntityHunterUvShieldSmokeStub),
			&smokeTrampoline,
			"defensive smoke"))
	{
		MH_RemoveHook(updateSite);
		MH_RemoveHook(clientSite);
		return false;
	}
	if (!CreateHook(
			groundPoundSite,
			reinterpret_cast<void *>(&EngineEntityHunterUvShieldGroundPoundStub),
			&groundPoundTrampoline,
			"ground pound"))
	{
		MH_RemoveHook(updateSite);
		MH_RemoveHook(clientSite);
		MH_RemoveHook(smokeSite);
		return false;
	}

	g_engine_entity_hunter_uv_shield_timer_trampoline = timerTrampoline;
	g_engine_entity_hunter_uv_shield_client_trampoline = clientTrampoline;
	g_engine_entity_hunter_uv_shield_smoke_trampoline = smokeTrampoline;
	g_engine_entity_hunter_uv_shield_ground_pound_trampoline = groundPoundTrampoline;

	if (MH_EnableHook(updateSite) != MH_OK ||
		MH_EnableHook(clientSite) != MH_OK ||
		MH_EnableHook(smokeSite) != MH_OK ||
		MH_EnableHook(groundPoundSite) != MH_OK)
	{
		RemoveHooks(updateSite, clientSite, smokeSite, groundPoundSite);
		g_engine_entity_hunter_uv_shield_timer_trampoline = nullptr;
		g_engine_entity_hunter_uv_shield_client_trampoline = nullptr;
		g_engine_entity_hunter_uv_shield_smoke_trampoline = nullptr;
		g_engine_entity_hunter_uv_shield_ground_pound_trampoline = nullptr;
		return false;
	}
	g_installed.store(true, std::memory_order_release);
	return true;
#endif
}

bool EngineHunterUvShieldRuntime::IsInstalled() noexcept
{
	return g_installed.load(std::memory_order_acquire);
}

void EngineHunterUvShieldRuntime::Shutdown() noexcept
{
#if defined(_M_X64)
	const std::lock_guard<std::mutex> guard(g_installMutex);
	if (g_installed.exchange(false, std::memory_order_acq_rel))
	{
		const HMODULE game = EngineEntityBindings::GameModule();
		if (game != nullptr)
		{
			RemoveHooks(
				reinterpret_cast<std::uint8_t *>(game) +
					EngineEntityBindings::HunterUvShieldUpdateSiteRva,
				reinterpret_cast<std::uint8_t *>(game) +
					EngineEntityBindings::HunterUvShieldClientSiteRva,
				reinterpret_cast<std::uint8_t *>(game) +
					EngineEntityBindings::HunterUvShieldSmokeSiteRva,
				reinterpret_cast<std::uint8_t *>(game) +
					EngineEntityBindings::HunterUvShieldGroundPoundSiteRva);
		}
	}
	g_engine_entity_hunter_uv_shield_timer_trampoline = nullptr;
	g_engine_entity_hunter_uv_shield_client_trampoline = nullptr;
	g_engine_entity_hunter_uv_shield_smoke_trampoline = nullptr;
	g_engine_entity_hunter_uv_shield_ground_pound_trampoline = nullptr;
	g_chargeOverride.store(-1, std::memory_order_release);
	g_durationOverride.store(0.0F, std::memory_order_release);
	AcquireSRWLockExclusive(&g_stateLock);
	g_state = {};
	ReleaseSRWLockExclusive(&g_stateLock);
#endif
}

bool EngineHunterUvShieldRuntime::Read(DlModHunterUvShieldV1 &snapshot) noexcept
{
	snapshot = {};
	snapshot.structSize = sizeof(snapshot);
	snapshot.abiVersion = MODSDK_ABI_VERSION;
	if (!g_installed.load(std::memory_order_acquire))
		return false;

	AcquireSRWLockShared(&g_stateLock);
	ShieldState state = g_state;
	ReleaseSRWLockShared(&g_stateLock);
	if (state.stateMachine == nullptr)
		return false;

	std::int32_t zombieState = 0;
	float shieldSeconds = 0.0F;
	std::uint8_t shieldFlag = 0;
	const bool hasState = ReadField(
		state.stateMachine,
		EngineEntityBindings::ZombieStateMachineStateOffset,
		zombieState);
	const bool hasShield = ReadField(
							   state.stateMachine,
							   EngineEntityBindings::ZombieStateMachineShieldSecondsOffset,
							   shieldSeconds) &&
						   std::isfinite(shieldSeconds);
	const bool hasFlag = ReadField(
		state.stateMachine,
		EngineEntityBindings::ZombieStateMachineShieldFlagOffset,
		shieldFlag);
	std::int32_t smokeCharges = 0;
	const bool hasCharges = ReadField(
		state.stateMachine,
		EngineEntityBindings::ZombieStateMachineSmokeChargesOffset,
		smokeCharges);

	snapshot.stateMachine = state.stateMachine;
	snapshot.zombieState = hasState
							   ? static_cast<std::uint32_t>(zombieState)
							   : 0U;
	snapshot.shieldSeconds = hasShield ? shieldSeconds : 0.0F;
	snapshot.shieldDuration = state.durationSeconds;
	snapshot.shieldFlag = hasFlag ? shieldFlag : 0U;
	snapshot.smokeCharges = hasCharges ? smokeCharges : 0;
	if (hasState)
		snapshot.validFields |= DLMOD_HUNTER_UV_HAS_STATE;
	if (hasShield)
		snapshot.validFields |= DLMOD_HUNTER_UV_HAS_SHIELD;
	if (hasFlag)
		snapshot.validFields |= DLMOD_HUNTER_UV_HAS_FLAG;
	if (hasCharges)
		snapshot.validFields |= DLMOD_HUNTER_UV_HAS_CHARGES;

	double now = 0.0;
	snapshot.readTimeSeconds = ReadTimeSeconds(now) ? now : 0.0;
	return snapshot.validFields != 0;
}

DlModResult EngineHunterUvShieldRuntime::SetOverride(
	uint32_t mode,
	float value) noexcept
{
	if (mode > DLMOD_HUNTER_UV_SHIELD_MODE_SCALE || !std::isfinite(value))
		return DLMOD_RESULT_INVALID_ARGUMENT;
	float sanitized = 0.0F;
	if (mode == DLMOD_HUNTER_UV_SHIELD_MODE_PIN)
		sanitized = SanitizeShieldSeconds(value);
	else if (mode == DLMOD_HUNTER_UV_SHIELD_MODE_SCALE)
		sanitized = (std::min)((std::max)(value, 0.0F), kMaximumScale);
	g_overrideValue.store(sanitized, std::memory_order_release);
	g_mode.store(mode, std::memory_order_release);
	return g_installed.load(std::memory_order_acquire)
			   ? DLMOD_RESULT_SUCCESS
			   : DLMOD_RESULT_INVALID_STATE;
}
DlModResult EngineHunterUvShieldRuntime::SetChargeOverride(int32_t charges) noexcept
{
	if (charges < -1 || charges > kMaximumCharges)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	g_chargeOverride.store(charges, std::memory_order_release);
	return g_installed.load(std::memory_order_acquire)
			   ? DLMOD_RESULT_SUCCESS
			   : DLMOD_RESULT_INVALID_STATE;
}

DlModResult EngineHunterUvShieldRuntime::SetDurationOverride(float seconds) noexcept
{
	if (!std::isfinite(seconds) || seconds > kMaximumShieldSeconds)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	g_durationOverride.store(seconds > 0.0F ? seconds : 0.0F, std::memory_order_release);
	return g_installed.load(std::memory_order_acquire)
			   ? DLMOD_RESULT_SUCCESS
			   : DLMOD_RESULT_INVALID_STATE;
}
