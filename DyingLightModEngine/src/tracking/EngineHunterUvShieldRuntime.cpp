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
	void *g_engine_entity_hunter_uv_shield_drain_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_shield_client_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_shield_smoke_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_shield_ground_pound_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_shield_smoke_blocked_continuation = nullptr;
	void *g_engine_entity_hunter_uv_shield_ground_pound_blocked_continuation = nullptr;
	void *g_engine_entity_hunter_uv_shield_hud_container_trampoline = nullptr;
	void *g_engine_entity_hunter_uv_shield_hud_ability_trampoline = nullptr;
	void EngineEntityHunterUvShieldTimerStub();
	void EngineEntityHunterUvShieldDrainStub();
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
	constexpr std::uint8_t kDrainSignature[] = {
		0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
		0xEC, 0x20, 0xF3, 0x0F, 0x58, 0x49, 0x10};
	constexpr std::uint8_t kHudContainerSignature[] = {
		0x48, 0x89, 0x5C, 0x24, 0x18, 0x57, 0x48, 0x83,
		0xEC, 0x40, 0x48, 0x8B, 0x05};
	constexpr std::uint8_t kHudAbilitySignature[] = {
		0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
		0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18};
	constexpr float kMaximumShieldSeconds = 3600.0F;
	constexpr float kMaximumScale = 100.0F;
	constexpr std::int32_t kMaximumCharges = 99;
	constexpr std::int32_t kDisableChargeOverride = -1;
	constexpr std::int32_t kUnlimitedChargeOverride = -2;

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
	std::atomic<std::int32_t> g_chargeOverride{kDisableChargeOverride};
	std::atomic<float> g_durationOverride{0.0F};
	std::atomic<float> g_hudProgressRatio{-1.0F};
	std::atomic<void *> g_hudShieldAbility{nullptr};
	std::atomic<void *> g_hudContainer{nullptr};
	std::atomic<void *> g_customTakeoverStateMachine{nullptr};
	std::atomic_bool g_hudContainerLogged = false;
	std::atomic_uint32_t g_hudAbilityLogCount{0};
	std::atomic_bool g_installed = false;
	SRWLOCK g_hookStateLock = SRWLOCK_INIT;
	DlModHunterUvShieldHookDescV1 g_hookState{};
	std::mutex g_installMutex;
	LARGE_INTEGER g_counterFrequency{};
	INIT_ONCE g_counterFrequencyOnce = INIT_ONCE_STATIC_INIT;

	const DlModHunterUvShieldHookDescV1 ReadHookState() noexcept
	{
		DlModHunterUvShieldHookDescV1 hooks{};
		AcquireSRWLockShared(&g_hookStateLock);
		hooks = g_hookState;
		ReleaseSRWLockShared(&g_hookStateLock);
		return hooks;
	}

	bool InvokeStartHook(
		void *stateMachine,
		std::uint32_t source,
		float durationSeconds) noexcept
	{
		const DlModHunterUvShieldHookDescV1 hooks = ReadHookState();
		if (hooks.onStart == nullptr)
			return true;
		try
		{
			const bool allowOriginal = hooks.onStart(
				stateMachine,
				source,
				durationSeconds,
				hooks.userData) != 0;
			if (!allowOriginal) {
				g_customTakeoverStateMachine.store(
					stateMachine,
					std::memory_order_release);
			}
			return allowOriginal;
		}
		catch (...)
		{
			EngineLogger::Warning("Hunter UV shield start hook threw an exception.");
			return true;
		}
	}

	void InvokeUpdateHook(
		void *stateMachine,
		std::uint32_t source,
		float previousSeconds,
		float remainingSeconds) noexcept
	{
		const DlModHunterUvShieldHookDescV1 hooks = ReadHookState();
		if (hooks.onUpdate == nullptr)
			return;
		try
		{
			hooks.onUpdate(
				stateMachine,
				source,
				previousSeconds,
				remainingSeconds,
				hooks.userData);
		}
		catch (...)
		{
			EngineLogger::Warning("Hunter UV shield update hook threw an exception.");
		}
	}

	void InvokeEndHook(
		void *stateMachine,
		std::uint32_t source) noexcept
	{
		const DlModHunterUvShieldHookDescV1 hooks = ReadHookState();
		if (hooks.onEnd == nullptr)
			return;
		try
		{
			hooks.onEnd(stateMachine, source, hooks.userData);
			if (g_customTakeoverStateMachine.load(std::memory_order_acquire) ==
				stateMachine) {
				g_customTakeoverStateMachine.store(
					nullptr,
					std::memory_order_release);
			}
		}
		catch (...)
		{
			EngineLogger::Warning("Hunter UV shield end hook threw an exception.");
		}
	}

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

	template <typename Value>
	bool WriteField(void *object, std::uintptr_t offset, const Value &value) noexcept
	{
		if (object == nullptr)
			return false;
		__try
		{
			*reinterpret_cast<Value *>(
				static_cast<std::uint8_t *>(object) + offset) = value;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
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
		if (charges == kDisableChargeOverride)
			return;
		const std::int32_t storedCharges =
			charges == kUnlimitedChargeOverride ? -1 : charges;
		__try
		{
			*reinterpret_cast<std::int32_t*>(
				static_cast<std::uint8_t*>(stateMachine) +
				EngineEntityBindings::ZombieStateMachineSmokeChargesOffset) = storedCharges;
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

	float ApplyTimerLifecycle(
		void *stateMachine,
		std::uint32_t source,
		float value) noexcept
	{
		float previous = 0.0F;
		ReadField(
			stateMachine,
			EngineEntityBindings::ZombieStateMachineShieldSecondsOffset,
			previous);
		if (g_customTakeoverStateMachine.load(std::memory_order_acquire) ==
			stateMachine) {
			InvokeUpdateHook(stateMachine, source, previous, previous);
			if (previous > 0.0F) {
				RaiseShieldFlag(stateMachine);
			}
			return previous;
		}
		const float result = ApplyTimerOverride(value);
		InvokeUpdateHook(stateMachine, source, previous, result);
		if (previous > 0.0F && result <= 0.0F)
			InvokeEndHook(stateMachine, source);
		return result;
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

	using HudContainerFunction = char(__fastcall *)(void *);
	using HudAbilityFunction = void(__fastcall *)(void *, void *);
	using UiSetVisibleFunction = void(__fastcall *)(void *, bool);

	UiSetVisibleFunction ResolveUiSetVisible() noexcept
	{
		const HMODULE game = EngineEntityBindings::GameModule();
		if (game == nullptr)
			return nullptr;
		__try
		{
			return *reinterpret_cast<UiSetVisibleFunction *>(
				reinterpret_cast<std::uintptr_t>(game) +
				EngineEntityBindings::UiElementSetVisibleImportRva);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}

	void CaptureHunterUvShieldHudAbility(void *hud) noexcept
	{
		void *ability = nullptr;
		std::uint32_t count = 0;
		if (ReadField(hud, 0x68, count) && count > 4)
			ReadField(hud, 0x60 + 4 * sizeof(void *), ability);
		g_hudContainer.store(hud, std::memory_order_release);
		g_hudShieldAbility.store(ability, std::memory_order_release);
		if (!g_hudContainerLogged.exchange(true, std::memory_order_acq_rel))
		{
			EngineLogger::Logf(
				EngineLogLevel::Info,
				"Hunter UV HUD container entered (count=%u, uvCandidate=%p)",
				static_cast<unsigned int>(count),
				ability);
		}
	}

	void RefreshHunterUvShieldHudAbility() noexcept
	{
		void *container = g_hudContainer.load(std::memory_order_acquire);
		if (container == nullptr)
			return;
		std::uint32_t count = 0;
		if (!ReadField(container, 0x68, count) || count <= 4)
			return;
		void *ability = nullptr;
		if (ReadField(container, 0x60 + 4 * sizeof(void *), ability) &&
			ability != nullptr) {
			g_hudShieldAbility.store(ability, std::memory_order_release);
		}
	}

	bool IsHunterUvShieldHudAbility(void *ability) noexcept
	{
		if (ability == nullptr)
			return false;
		if (ability == g_hudShieldAbility.load(std::memory_order_acquire))
			return true;

		void *container = g_hudContainer.load(std::memory_order_acquire);
		if (container == nullptr)
			return false;
		std::uint32_t count = 0;
		if (!ReadField(container, 0x68, count) || count <= 4)
			return false;

		void *candidate = nullptr;
		if (!ReadField(
				container,
				0x60 + 4 * sizeof(void *),
				candidate) ||
			candidate != ability) {
			return false;
		}
		g_hudShieldAbility.store(ability, std::memory_order_release);
		return true;
	}

	void SetHunterUvShieldHudNumberVisible(void *ability, bool visible) noexcept
	{
		void *text = nullptr;
		if (!ReadField(ability, 0x30, text) || text == nullptr)
			return;
		const UiSetVisibleFunction setVisible = ResolveUiSetVisible();
		if (setVisible == nullptr)
			return;
		__try
		{
			setVisible(text, visible);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	void SetHunterUvShieldHudFillVisible(void *ability, bool visible) noexcept
	{
		void *fill = nullptr;
		if (!ReadField(ability, 0x28, fill) || fill == nullptr)
			return;
		const UiSetVisibleFunction setVisible = ResolveUiSetVisible();
		if (setVisible == nullptr)
			return;
		__try
		{
			setVisible(fill, visible);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
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
	const float result = ApplyTimerLifecycle(
		stateMachine,
		DLMOD_HUNTER_UV_HOOK_SOURCE_UPDATE,
		value);
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
	if (g_customTakeoverStateMachine.load(std::memory_order_acquire) ==
		stateMachine) {
		float takeoverSeconds = 0.0F;
		ReadField(
			stateMachine,
			EngineEntityBindings::ZombieStateMachineShieldSecondsOffset,
			takeoverSeconds);
		InvokeUpdateHook(
			stateMachine,
			DLMOD_HUNTER_UV_HOOK_SOURCE_CLIENT_UPDATE,
			takeoverSeconds,
			takeoverSeconds);
		if (takeoverSeconds > 0.0F) {
			RaiseShieldFlag(stateMachine);
		}
		return takeoverSeconds;
	}
	const std::uint32_t mode = g_mode.load(std::memory_order_acquire);
	float result = value;
	if (mode == DLMOD_HUNTER_UV_SHIELD_MODE_OFF)
		result = 0.0F;
	else if (mode == DLMOD_HUNTER_UV_SHIELD_MODE_PIN)
	{
		const float setting = g_overrideValue.load(std::memory_order_acquire);
		if (std::isfinite(setting) && setting > value)
			result = setting;
	}
	float previous = 0.0F;
	ReadField(
		stateMachine,
		EngineEntityBindings::ZombieStateMachineShieldSecondsOffset,
		previous);
	InvokeUpdateHook(
		stateMachine,
		DLMOD_HUNTER_UV_HOOK_SOURCE_CLIENT_UPDATE,
		previous,
		result);
	if (previous > 0.0F && result <= 0.0F)
		InvokeEndHook(
			stateMachine,
			DLMOD_HUNTER_UV_HOOK_SOURCE_CLIENT_UPDATE);
	return result;
}

extern "C" float EngineEntity_OnHunterUvShieldDefensiveSmoke(
	void *stateMachine,
	std::uint32_t durationBits) noexcept
{
	const float duration = BitsToFloat(durationBits);
	if (stateMachine == nullptr || !std::isfinite(duration))
		return duration;
	if (!InvokeStartHook(
			stateMachine,
			DLMOD_HUNTER_UV_HOOK_SOURCE_SMOKE,
			duration))
		return -1.0F;
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
	if (!InvokeStartHook(
			stateMachine,
			DLMOD_HUNTER_UV_HOOK_SOURCE_GROUND_POUND,
			duration))
		return -1.0F;
	RecordObservation(stateMachine, duration, true);
	return ApplyDurationOverride(duration);
}

/* 护盾激活时把猎手 UV 能量的负向变化（紫外线扣减）压成 0，等于真正免疫紫外线。 */
extern "C" float EngineEntity_OnHunterUvShieldDrain(
	void* component,
	std::uint32_t deltaBits) noexcept
{
	const float delta = BitsToFloat(deltaBits);
	if (component == nullptr || !std::isfinite(delta) || delta >= 0.0F)
		return delta;
	if (g_mode.load(std::memory_order_acquire) != DLMOD_HUNTER_UV_SHIELD_MODE_PIN)
		return delta;
	if (g_overrideValue.load(std::memory_order_acquire) <= 0.0F)
		return delta;
	/* 该入口就是猎手 UV 组件的能量更新函数，避免依赖易失效的 owner 反查。 */
	return 0.0F;
}

extern "C" char __fastcall EngineEntity_OnHunterUvShieldHudContainer(void *hud) noexcept
{
	const auto original = reinterpret_cast<HudContainerFunction>(
		g_engine_entity_hunter_uv_shield_hud_container_trampoline);
	if (original == nullptr)
		return 0;
	CaptureHunterUvShieldHudAbility(hud);
	const char result = original(hud);
	CaptureHunterUvShieldHudAbility(hud);
	return result;
}

extern "C" void __fastcall EngineEntity_OnHunterUvShieldHudAbility(
	void *ability,
	void *data) noexcept
{
	const auto original = reinterpret_cast<HudAbilityFunction>(
		g_engine_entity_hunter_uv_shield_hud_ability_trampoline);
	if (original == nullptr)
		return;

	const float ratio = g_hudProgressRatio.load(std::memory_order_acquire);
	RefreshHunterUvShieldHudAbility();
	const bool hasRatio = std::isfinite(ratio) && ratio >= 0.0F && ratio <= 1.0F;
	std::int32_t displayedCharges = 0;
	ReadField(data, 0x00, displayedCharges);
	bool isShieldAbility = IsHunterUvShieldHudAbility(ability);
	/* 无限充能哨兵是 -1；用它作为兜底识别，避免能力顺序变化导致抓错项。 */
	if (!isShieldAbility && hasRatio && displayedCharges < 0)
	{
		g_hudShieldAbility.store(ability, std::memory_order_release);
		isShieldAbility = true;
	}
	const bool useOverride = isShieldAbility && hasRatio && data != nullptr;
	const std::uint32_t logIndex = g_hudAbilityLogCount.fetch_add(
		1, std::memory_order_relaxed);
	if (logIndex < 8)
	{
		EngineLogger::Logf(
			EngineLogLevel::Info,
			"Hunter UV HUD ability call #%u (ability=%p, charges=%d, ratio=%.3f, matched=%u)",
			static_cast<unsigned int>(logIndex),
			ability,
			static_cast<int>(displayedCharges),
			static_cast<double>(ratio),
			isShieldAbility ? 1U : 0U);
	}
	if (useOverride)
	{
		/* data + 0x0C 是该能力项的环形进度，data + 0x14 控制图标可见性。 */
		const float clampedRatio = (std::min)((std::max)(ratio, 0.0F), 1.0F);
		const std::uint8_t visible = 1;
		WriteField(data, 0x0C, clampedRatio);
		WriteField(data, 0x14, visible);
	}

	original(ability, data);
	if (isShieldAbility)
	{
		/* 原生更新会把 -1/充能数量写回文本；护盾 HUD 只保留图标和进度。 */
		SetHunterUvShieldHudNumberVisible(ability, !useOverride);
		if (useOverride)
			SetHunterUvShieldHudFillVisible(ability, true);
	}
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
	void *hudContainerSite = reinterpret_cast<std::uint8_t *>(game) +
						 EngineEntityBindings::HunterUvShieldHudContainerSiteRva;
	void *hudAbilitySite = reinterpret_cast<std::uint8_t *>(game) +
						 EngineEntityBindings::HunterUvShieldHudAbilitySiteRva;
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
	g_engine_entity_hunter_uv_shield_smoke_blocked_continuation =
		reinterpret_cast<std::uint8_t *>(game) +
		EngineEntityBindings::HunterUvShieldSmokeAfterFlagRva;
	g_engine_entity_hunter_uv_shield_ground_pound_blocked_continuation =
		reinterpret_cast<std::uint8_t *>(game) +
		EngineEntityBindings::HunterUvShieldGroundPoundAfterFlagRva;
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

	const bool hudSitesMatch =
		std::memcmp(
			hudContainerSite,
			kHudContainerSignature,
			sizeof(kHudContainerSignature)) == 0 &&
		std::memcmp(
			hudAbilitySite,
			kHudAbilitySignature,
			sizeof(kHudAbilitySignature)) == 0;
	if (hudSitesMatch)
	{
		void *hudContainerTrampoline = nullptr;
		void *hudAbilityTrampoline = nullptr;
		if (CreateHook(
				hudContainerSite,
				reinterpret_cast<void *>(&EngineEntity_OnHunterUvShieldHudContainer),
				&hudContainerTrampoline,
				"HudZombieAbilities container") &&
			CreateHook(
				hudAbilitySite,
				reinterpret_cast<void *>(&EngineEntity_OnHunterUvShieldHudAbility),
				&hudAbilityTrampoline,
				"HudZombieAbilities ability"))
		{
			g_engine_entity_hunter_uv_shield_hud_container_trampoline =
				hudContainerTrampoline;
			g_engine_entity_hunter_uv_shield_hud_ability_trampoline =
				hudAbilityTrampoline;
		}
		else
		{
			MH_RemoveHook(hudContainerSite);
			MH_RemoveHook(hudAbilitySite);
			EngineLogger::Warning(
				"Hunter UV shield HUD ability hook unavailable; native HUD remains active");
		}
	}
	else
	{
		EngineLogger::Warning(
			"Hunter UV shield HUD ability sites did not match; native HUD remains active");
	}

	/* 能量扣减入口是可选钩子：签名不符或挂接失败只会失去真实免疫，HUD 计时仍然工作。 */
	void* drainSite = reinterpret_cast<std::uint8_t*>(game) +
		EngineEntityBindings::HunterUvDrainSiteRva;
	if (std::memcmp(drainSite, kDrainSignature, sizeof(kDrainSignature)) == 0)
	{
		void* drainTrampoline = nullptr;
		if (CreateHook(
			drainSite,
			reinterpret_cast<void*>(&EngineEntityHunterUvShieldDrainStub),
			&drainTrampoline,
			"uv drain"))
		{
			g_engine_entity_hunter_uv_shield_drain_trampoline = drainTrampoline;
			if (MH_EnableHook(drainSite) != MH_OK)
			{
				g_engine_entity_hunter_uv_shield_drain_trampoline = nullptr;
				MH_RemoveHook(drainSite);
				EngineLogger::Warning(
					"Hunter UV shield drain hook failed; UV drain will not be blocked");
			}
		}
		else
		{
			EngineLogger::Warning(
				"Hunter UV shield drain hook failed; UV drain will not be blocked");
		}
	}
	else
	{
		EngineLogger::Warning(
			"Hunter UV shield drain site did not match; UV drain will not be blocked");
	}

	MH_STATUS enableStatus = MH_EnableHook(updateSite);
	if (enableStatus == MH_OK)
		enableStatus = MH_EnableHook(clientSite);
	if (enableStatus == MH_OK)
		enableStatus = MH_EnableHook(smokeSite);
	if (enableStatus == MH_OK)
		enableStatus = MH_EnableHook(groundPoundSite);
	if (enableStatus == MH_OK &&
		g_engine_entity_hunter_uv_shield_hud_container_trampoline != nullptr)
		enableStatus = MH_EnableHook(hudContainerSite);
	if (enableStatus == MH_OK &&
		g_engine_entity_hunter_uv_shield_hud_ability_trampoline != nullptr)
		enableStatus = MH_EnableHook(hudAbilitySite);
	if (enableStatus != MH_OK)
	{
		RemoveHooks(updateSite, clientSite, smokeSite, groundPoundSite);
		MH_DisableHook(hudContainerSite);
		MH_RemoveHook(hudContainerSite);
		MH_DisableHook(hudAbilitySite);
		MH_RemoveHook(hudAbilitySite);
		if (g_engine_entity_hunter_uv_shield_drain_trampoline != nullptr)
		{
			MH_DisableHook(drainSite);
			MH_RemoveHook(drainSite);
		}
		g_engine_entity_hunter_uv_shield_timer_trampoline = nullptr;
		g_engine_entity_hunter_uv_shield_drain_trampoline = nullptr;
		g_engine_entity_hunter_uv_shield_client_trampoline = nullptr;
		g_engine_entity_hunter_uv_shield_smoke_trampoline = nullptr;
		g_engine_entity_hunter_uv_shield_ground_pound_trampoline = nullptr;
		g_engine_entity_hunter_uv_shield_smoke_blocked_continuation = nullptr;
		g_engine_entity_hunter_uv_shield_ground_pound_blocked_continuation = nullptr;
		g_engine_entity_hunter_uv_shield_hud_container_trampoline = nullptr;
		g_engine_entity_hunter_uv_shield_hud_ability_trampoline = nullptr;
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
			if (g_engine_entity_hunter_uv_shield_hud_container_trampoline != nullptr)
			{
				void *hudContainerSite = reinterpret_cast<std::uint8_t *>(game) +
					EngineEntityBindings::HunterUvShieldHudContainerSiteRva;
				MH_DisableHook(hudContainerSite);
				MH_RemoveHook(hudContainerSite);
			}
			if (g_engine_entity_hunter_uv_shield_hud_ability_trampoline != nullptr)
			{
				void *hudAbilitySite = reinterpret_cast<std::uint8_t *>(game) +
					EngineEntityBindings::HunterUvShieldHudAbilitySiteRva;
				MH_DisableHook(hudAbilitySite);
				MH_RemoveHook(hudAbilitySite);
			}
			if (g_engine_entity_hunter_uv_shield_drain_trampoline != nullptr)
			{
				void* drainSite = reinterpret_cast<std::uint8_t*>(game) +
					EngineEntityBindings::HunterUvDrainSiteRva;
				MH_DisableHook(drainSite);
				MH_RemoveHook(drainSite);
			}

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
	g_engine_entity_hunter_uv_shield_drain_trampoline = nullptr;
	g_engine_entity_hunter_uv_shield_client_trampoline = nullptr;
	g_engine_entity_hunter_uv_shield_smoke_trampoline = nullptr;
	g_engine_entity_hunter_uv_shield_ground_pound_trampoline = nullptr;
	g_engine_entity_hunter_uv_shield_smoke_blocked_continuation = nullptr;
	g_engine_entity_hunter_uv_shield_ground_pound_blocked_continuation = nullptr;
	g_engine_entity_hunter_uv_shield_hud_container_trampoline = nullptr;
	g_engine_entity_hunter_uv_shield_hud_ability_trampoline = nullptr;
	g_chargeOverride.store(kDisableChargeOverride, std::memory_order_release);
	g_durationOverride.store(0.0F, std::memory_order_release);
	g_hudProgressRatio.store(-1.0F, std::memory_order_release);
	g_hudShieldAbility.store(nullptr, std::memory_order_release);
	g_hudContainer.store(nullptr, std::memory_order_release);
	g_customTakeoverStateMachine.store(nullptr, std::memory_order_release);
	g_hudContainerLogged.store(false, std::memory_order_release);
	g_hudAbilityLogCount.store(0, std::memory_order_release);
	AcquireSRWLockExclusive(&g_hookStateLock);
	g_hookState = {};
	ReleaseSRWLockExclusive(&g_hookStateLock);
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
	if ((charges < 0 && charges != kDisableChargeOverride &&
		 charges != kUnlimitedChargeOverride) ||
		charges > kMaximumCharges)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	g_chargeOverride.store(charges, std::memory_order_release);
	return g_installed.load(std::memory_order_acquire)
			   ? DLMOD_RESULT_SUCCESS
			   : DLMOD_RESULT_INVALID_STATE;
}

DlModResult EngineHunterUvShieldRuntime::SetProgressRatio(float ratio) noexcept
{
	if (!std::isfinite(ratio) || ratio < -1.0F || ratio > 1.0F)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	if (!g_installed.load(std::memory_order_acquire) ||
		g_engine_entity_hunter_uv_shield_hud_ability_trampoline == nullptr)
	{
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}
	g_hudProgressRatio.store(ratio, std::memory_order_release);
	return DLMOD_RESULT_SUCCESS;
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

DlModResult EngineHunterUvShieldRuntime::RegisterHooks(
	const DlModHunterUvShieldHookDescV1 &desc) noexcept
{
	if (desc.structSize < sizeof(DlModHunterUvShieldHookDescV1) ||
		desc.abiVersion != MODSDK_ABI_VERSION ||
		(desc.onStart == nullptr && desc.onUpdate == nullptr && desc.onEnd == nullptr))
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (!g_installed.load(std::memory_order_acquire))
		return DLMOD_RESULT_INVALID_STATE;

	AcquireSRWLockExclusive(&g_hookStateLock);
	if (g_hookState.onStart != nullptr ||
		g_hookState.onUpdate != nullptr ||
		g_hookState.onEnd != nullptr)
	{
		ReleaseSRWLockExclusive(&g_hookStateLock);
		return DLMOD_RESULT_INVALID_STATE;
	}
	g_hookState = desc;
	ReleaseSRWLockExclusive(&g_hookStateLock);
	return DLMOD_RESULT_SUCCESS;
}

DlModResult EngineHunterUvShieldRuntime::UnregisterHooks() noexcept
{
	AcquireSRWLockExclusive(&g_hookStateLock);
	g_hookState = {};
	ReleaseSRWLockExclusive(&g_hookStateLock);
	return DLMOD_RESULT_SUCCESS;
}

DlModResult EngineHunterUvShieldRuntime::SetTimer(
	void *stateMachine,
	float seconds) noexcept
{
	if (stateMachine == nullptr || !std::isfinite(seconds) || seconds < 0.0F)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	if (!g_installed.load(std::memory_order_acquire))
		return DLMOD_RESULT_INVALID_STATE;

	const float sanitized = SanitizeShieldSeconds(seconds);
	if (!WriteField(
			stateMachine,
			EngineEntityBindings::ZombieStateMachineShieldSecondsOffset,
			sanitized))
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	const std::uint8_t flag = sanitized > 0.0F ? 1U : 0U;
	WriteField(
		stateMachine,
		EngineEntityBindings::ZombieStateMachineShieldFlagOffset,
		flag);
	RecordObservation(stateMachine, sanitized, sanitized > 0.0F);
	return DLMOD_RESULT_SUCCESS;
}
