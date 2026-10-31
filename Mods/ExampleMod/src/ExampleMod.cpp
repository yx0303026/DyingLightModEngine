#include "sdk/EngineEntityAbi.h"
#include "sdk/EngineHostAbi.h"
#include "sdk/EngineModLifecycleAbi.h"
#include "logging/EngineLogger.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace {

	/* 示例模组概览：拦截原版紫外线护盾开始事件，使用自己的能量和计时逻辑。 */
	constexpr float kShieldFullDurationSeconds = 6.0F;
	constexpr float kShieldFullRechargeSeconds = 20.0F;
	constexpr std::int32_t kUnlimitedCharges = -2;
	constexpr float kMaximumFrameSeconds = 0.1F;

	struct ExampleModState {
		void* hostContext = nullptr;
		EngineLogCallback hostLog = nullptr;
		const DlModEntityApiV1* entity = nullptr;
		void* stateMachine = nullptr;
		bool uvShieldAvailable = false;
		bool shieldHooksRegistered = false;
		bool shieldActive = false;
		bool depletionLogged = false;
		bool hudProgressWarningLogged = false;
		float energy = 1.0F;
		float remainingSeconds = 0.0F;
		ULONGLONG lastTickMs = 0;
	};

	ExampleModState g_exampleModState;

	void Log(EngineLogLevel level, const char* message)
	{
		if (g_exampleModState.hostLog == nullptr) {
			return;
		}
		g_exampleModState.hostLog(
			g_exampleModState.hostContext,
			static_cast<uint32_t>(level),
			message);
	}

	void LogPercent(EngineLogLevel level, const char* format, float ratio)
	{
		char buffer[160]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			format,
			static_cast<double>(ratio) * 100.0);
		Log(level, buffer);
	}

	bool IsHunterActive()
	{
		const ExampleModState& state = g_exampleModState;
		if (state.entity == nullptr ||
			!state.uvShieldAvailable ||
			state.entity->readHunterUvShield == nullptr) {
			return false;
		}
		DlModHunterUvShieldV1 snapshot{};
		snapshot.structSize = sizeof(snapshot);
		snapshot.abiVersion = MODSDK_ABI_VERSION;
		return state.entity->readHunterUvShield(&snapshot) == DLMOD_RESULT_SUCCESS;
	}

	void SetCustomTimer(ExampleModState& state, float seconds)
	{
		if (state.entity == nullptr ||
			state.entity->setHunterUvShieldTimer == nullptr ||
			state.stateMachine == nullptr) {
			return;
		}
		state.entity->setHunterUvShieldTimer(state.stateMachine, seconds);
	}

	void UpdateHudProgress(ExampleModState& state)
	{
		if (state.entity == nullptr ||
			state.entity->setHunterUvShieldProgress == nullptr) {
			return;
		}
		if (state.entity->setHunterUvShieldProgress(state.energy) != DLMOD_RESULT_SUCCESS &&
			!state.hudProgressWarningLogged) {
			state.hudProgressWarningLogged = true;
			Log(EngineLogLevel::Warning, "自定义紫外线护盾 HUD 进度不可用。");
		}
	}

	void BeginCustomShield(
		ExampleModState& state,
		void* stateMachine,
		float sourceDuration)
	{
		state.stateMachine = stateMachine;
		state.shieldActive = true;
		state.depletionLogged = false;
		state.remainingSeconds =
			(std::min)(kShieldFullDurationSeconds, state.energy * kShieldFullDurationSeconds);
		if (state.remainingSeconds <= 0.0F) {
			state.shieldActive = false;
			state.remainingSeconds = 0.0F;
			SetCustomTimer(state, 0.0F);
			Log(EngineLogLevel::Warning, "自定义紫外线护盾能量为空，阻止原版护盾。");
			return;
		}

		state.entity->setHunterUvShieldCharges(kUnlimitedCharges);
		SetCustomTimer(state, state.remainingSeconds);
		UpdateHudProgress(state);
		LogPercent(
			EngineLogLevel::Info,
			"自定义紫外线护盾开始，原版逻辑已跳过（能量 %.0f%%，原版时长 %.2fs）。",
			state.energy);
		(void)sourceDuration;
	}

	uint8_t MODSDK_CALL OnUvShieldStart(
		void* stateMachine,
		uint32_t source,
		float durationSeconds,
		void* userData)
	{
		ExampleModState* state = static_cast<ExampleModState*>(userData);
		if (state != &g_exampleModState ||
			state->entity == nullptr ||
			(source != DLMOD_HUNTER_UV_HOOK_SOURCE_SMOKE &&
				source != DLMOD_HUNTER_UV_HOOK_SOURCE_GROUND_POUND)) {
			return 1U;
		}

		BeginCustomShield(*state, stateMachine, durationSeconds);
		state->lastTickMs = GetTickCount64();
		return 0U;
	}

	void MODSDK_CALL OnUvShieldUpdate(
		void* stateMachine,
		uint32_t source,
		float previousSeconds,
		float remainingSeconds,
		void* userData)
	{
		ExampleModState* state = static_cast<ExampleModState*>(userData);
		if (state != &g_exampleModState ||
			state->stateMachine != stateMachine ||
			(state->shieldActive && state->remainingSeconds > 0.0F)) {
			return;
		}
		(void)source;
		(void)previousSeconds;
		(void)remainingSeconds;
	}

	void MODSDK_CALL OnUvShieldEnd(
		void* stateMachine,
		uint32_t source,
		void* userData)
	{
		ExampleModState* state = static_cast<ExampleModState*>(userData);
		if (state != &g_exampleModState ||
			state->stateMachine != stateMachine ||
			state->remainingSeconds > 0.0F) {
			return;
		}
		state->shieldActive = false;
		state->remainingSeconds = 0.0F;
		(void)source;
	}

	void UpdateCustomShield(ExampleModState& state, float deltaSeconds)
	{
		if (!state.shieldActive) {
			state.energy = (std::min)(
				1.0F,
				state.energy + deltaSeconds / kShieldFullRechargeSeconds);
			return;
		}

		state.remainingSeconds = (std::max)(
			0.0F,
			state.remainingSeconds - deltaSeconds);
		state.energy = (std::max)(
			0.0F,
			state.remainingSeconds / kShieldFullDurationSeconds);
		SetCustomTimer(state, state.remainingSeconds);
		if (state.remainingSeconds <= 0.0F) {
			state.shieldActive = false;
			if (!state.depletionLogged) {
				state.depletionLogged = true;
				Log(EngineLogLevel::Info, "自定义紫外线护盾结束，能量开始恢复。");
			}
		}
	}

	void MODSDK_CALL OnRenderFrame(void* modContext, void* graphicsContext)
	{
		(void)graphicsContext;
		ExampleModState& state = g_exampleModState;
		if (modContext != &state || !state.uvShieldAvailable || !IsHunterActive()) {
			return;
		}

		const ULONGLONG now = GetTickCount64();
		float deltaSeconds = 0.0F;
		if (state.lastTickMs != 0) {
			deltaSeconds = static_cast<float>(now - state.lastTickMs) / 1000.0F;
			deltaSeconds = (std::min)(deltaSeconds, kMaximumFrameSeconds);
		}
		state.lastTickMs = now;

		UpdateCustomShield(state, deltaSeconds);
		UpdateHudProgress(state);
	}

	DlModResult MODSDK_CALL OnLoad(
		void* modContext,
		const DlModHostApiV1* hostApi)
	{
		if (modContext != &g_exampleModState ||
			hostApi == nullptr ||
			hostApi->structSize < DLMOD_HOST_API_V1_ENTITY_SIZE ||
			hostApi->abiVersion != MODSDK_ABI_VERSION ||
			hostApi->log == nullptr ||
			hostApi->entity == nullptr ||
			hostApi->entity->structSize < DLMOD_ENTITY_API_V1_UV_TIMER_SIZE ||
			hostApi->entity->installUvShieldTracking == nullptr ||
			hostApi->entity->registerHunterUvShieldHooks == nullptr ||
			hostApi->entity->setHunterUvShieldTimer == nullptr) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		ExampleModState& state = g_exampleModState;
		state.hostContext = hostApi->context;
		state.hostLog = hostApi->log;
		state.entity = hostApi->entity;
		state.energy = 1.0F;
		state.hostLog(
			state.hostContext,
			static_cast<uint32_t>(EngineLogLevel::Info),
			"Example Mod 已加载，将接管紫外线护盾逻辑。");

		state.uvShieldAvailable =
			state.entity->installUvShieldTracking() == DLMOD_RESULT_SUCCESS;
		if (!state.uvShieldAvailable) {
			Log(EngineLogLevel::Warning, "紫外线护盾钩子不可用，模组不会接管原版逻辑。");
			return DLMOD_RESULT_SUCCESS;
		}

		DlModHunterUvShieldHookDescV1 hooks{};
		hooks.structSize = sizeof(hooks);
		hooks.abiVersion = MODSDK_ABI_VERSION;
		hooks.userData = &state;
		hooks.onStart = &OnUvShieldStart;
		hooks.onUpdate = &OnUvShieldUpdate;
		hooks.onEnd = &OnUvShieldEnd;
		if (state.entity->registerHunterUvShieldHooks(&hooks) != DLMOD_RESULT_SUCCESS) {
			Log(EngineLogLevel::Warning, "紫外线护盾生命周期钩子注册失败。");
			state.uvShieldAvailable = false;
			return DLMOD_RESULT_SUCCESS;
		}
		state.shieldHooksRegistered = true;
		Log(EngineLogLevel::Info, "紫外线护盾自定义逻辑已启用，无需自定义按键。");
		return DLMOD_RESULT_SUCCESS;
	}

	void MODSDK_CALL OnUnload(void* modContext)
	{
		if (modContext != &g_exampleModState) {
			return;
		}

		ExampleModState& state = g_exampleModState;
		if (state.entity != nullptr && state.shieldHooksRegistered &&
			state.entity->unregisterHunterUvShieldHooks != nullptr) {
			state.entity->unregisterHunterUvShieldHooks();
		}
		if (state.entity != nullptr && state.uvShieldAvailable) {
			SetCustomTimer(state, 0.0F);
			state.entity->setHunterUvShield(DLMOD_HUNTER_UV_SHIELD_MODE_GAME, 0.0F);
			state.entity->setHunterUvShieldCharges(-1);
			state.entity->setHunterUvShieldDuration(0.0F);
			state.entity->setHunterUvShieldProgress(-1.0F);
		}
		if (state.hostLog != nullptr) {
			state.hostLog(
				state.hostContext,
				static_cast<uint32_t>(EngineLogLevel::Info),
				"Example Mod 已卸载。");
		}
		g_exampleModState = {};
	}
}

extern "C" DlModResult MODSDK_CALL ModSDK_QueryMod(
	uint32_t requestedAbiVersion,
	DlModDescriptorV1* descriptor)
{
	if (descriptor == nullptr || descriptor->structSize < sizeof(DlModDescriptorV1)) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (requestedAbiVersion != MODSDK_ABI_VERSION) {
		return DLMOD_RESULT_UNSUPPORTED_ABI;
	}

	*descriptor = {
		sizeof(DlModDescriptorV1),
		MODSDK_ABI_VERSION,
		"example.mod",
		"Example Mod",
		"1.0.0",
		&g_exampleModState,
		&OnLoad,
		&OnUnload,
		"ExampleMod",
		&OnRenderFrame
	};
	return DLMOD_RESULT_SUCCESS;
}
