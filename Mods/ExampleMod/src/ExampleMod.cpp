#include "sdk/EngineEntityAbi.h"
#include "sdk/EngineHostAbi.h"
#include "sdk/EngineHotkeyAbi.h"
#include "sdk/EngineModLifecycleAbi.h"
#include "logging/EngineLogger.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace {

	/* 紫外线护盾改为可开关技能：满能量维持 10 秒，关闭后 15 秒回满，充能次数不再受限。 */
	constexpr float kShieldFullDurationSeconds = 6.0F;
	constexpr float kShieldFullRechargeSeconds = 20.0F;
	constexpr std::int32_t kUnlimitedCharges = -2;
	constexpr float kMaximumFrameSeconds = 0.1F;

	struct ExampleModState {
		void* hostContext = nullptr;
		EngineLogCallback hostLog = nullptr;
		const DlModEntityApiV1* entity = nullptr;
		const DlModHotkeyApiV1* hotkey = nullptr;
		bool uvShieldAvailable = false;
		bool shieldEnabled = false;
		bool shieldHotkeyRegistered = false;
		bool depletionLogged = false;
		bool hudProgressWarningLogged = false;
		float energy = 1.0F;
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
		std::snprintf(buffer, sizeof(buffer), format, static_cast<double>(ratio) * 100.0);
		Log(level, buffer);
	}

	bool IsHunterActive()
	{
		const ExampleModState& state = g_exampleModState;
		if (state.entity == nullptr || !state.uvShieldAvailable ||
			state.entity->readHunterUvShield == nullptr) {
			return false;
		}
		DlModHunterUvShieldV1 snapshot{};
		snapshot.structSize = sizeof(snapshot);
		snapshot.abiVersion = MODSDK_ABI_VERSION;
		return state.entity->readHunterUvShield(&snapshot) == DLMOD_RESULT_SUCCESS;
	}

	/* 技能开启时用 PIN 把光免疫计时钉在剩余能量对应的秒数上，关闭时强制清零。 */
	void ApplyShieldState()
	{
		ExampleModState& state = g_exampleModState;
		if (state.entity == nullptr || !state.uvShieldAvailable) {
			return;
		}
		state.entity->setHunterUvShieldCharges(kUnlimitedCharges);
		if (state.shieldEnabled && state.energy > 0.0F) {
			state.entity->setHunterUvShield(
				DLMOD_HUNTER_UV_SHIELD_MODE_PIN,
				state.energy * kShieldFullDurationSeconds);
		}
		else {
			state.entity->setHunterUvShield(DLMOD_HUNTER_UV_SHIELD_MODE_OFF, 0.0F);
		}
	}

	void ToggleShield()
	{
		ExampleModState& state = g_exampleModState;
		if (state.shieldEnabled) {
			state.shieldEnabled = false;
			state.depletionLogged = false;
			LogPercent(EngineLogLevel::Info, "UV shield disabled (energy %.0f%%).", state.energy);
			return;
		}
		if (state.energy <= 0.0F) {
			Log(EngineLogLevel::Warning, "UV shield cannot be enabled: energy is empty.");
			return;
		}
		state.shieldEnabled = true;
		state.depletionLogged = false;
		LogPercent(EngineLogLevel::Info, "UV shield enabled (energy %.0f%%).", state.energy);
	}

	void MODSDK_CALL OnShieldHotkey(void* userData, std::uint32_t actionId)
	{
		(void)actionId;
		if (userData != &g_exampleModState) {
			return;
		}
		ToggleShield();
	}

	void UpdateEnergy(float deltaSeconds)
	{
		ExampleModState& state = g_exampleModState;
		if (state.shieldEnabled) {
			state.energy -= deltaSeconds / kShieldFullDurationSeconds;
			if (state.energy <= 0.0F) {
				state.energy = 0.0F;
				state.shieldEnabled = false;
				if (!state.depletionLogged) {
					state.depletionLogged = true;
					Log(EngineLogLevel::Info, "UV shield depleted and recharging.");
				}
			}
			return;
		}
		state.energy = (std::min)(1.0F, state.energy + deltaSeconds / kShieldFullRechargeSeconds);
	}

	void MODSDK_CALL OnRenderFrame(void* modContext, void* graphicsContext)
	{
		(void)graphicsContext;
		ExampleModState& state = g_exampleModState;
		if (modContext != &state) {
			return;
		}

		const ULONGLONG now = GetTickCount64();
		float deltaSeconds = 0.0F;
		if (state.lastTickMs != 0) {
			deltaSeconds = static_cast<float>(now - state.lastTickMs) / 1000.0F;
			deltaSeconds = (std::min)(deltaSeconds, kMaximumFrameSeconds);
		}
		state.lastTickMs = now;

		/* 只在 BTZ 猎手存在时消耗/恢复能量，避免战役模式空转。 */
		if (!IsHunterActive()) {
			return;
		}
		UpdateEnergy(deltaSeconds);
		if (state.entity->setHunterUvShieldProgress(state.energy) != DLMOD_RESULT_SUCCESS &&
			!state.hudProgressWarningLogged) {
			state.hudProgressWarningLogged = true;
			Log(EngineLogLevel::Warning, "UV shield HUD progress hook is unavailable.");
		}
		ApplyShieldState();
	}

	DlModResult MODSDK_CALL OnLoad(void* modContext, const DlModHostApiV1* hostApi)
	{
		if (modContext != &g_exampleModState ||
			hostApi == nullptr ||
			hostApi->structSize < DLMOD_HOST_API_V1_MOUSE_STATE_SIZE ||
			hostApi->abiVersion != MODSDK_ABI_VERSION ||
			hostApi->log == nullptr) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		ExampleModState& state = g_exampleModState;
		state.hostContext = hostApi->context;
		state.hostLog = hostApi->log;
		state.hostLog(
			state.hostContext,
			static_cast<uint32_t>(EngineLogLevel::Info),
			"Example Mod loaded successfully.");
		state.energy = 1.0F;

		if (hostApi->structSize >= DLMOD_HOST_API_V1_ENTITY_SIZE &&
			hostApi->entity != nullptr &&
			hostApi->entity->structSize >= DLMOD_ENTITY_API_V1_UV_SHIELD_SIZE &&
			hostApi->entity->installUvShieldTracking != nullptr &&
			hostApi->entity->setHunterUvShieldProgress != nullptr) {
			state.entity = hostApi->entity;
			state.uvShieldAvailable =
				state.entity->installUvShieldTracking() == DLMOD_RESULT_SUCCESS;
			if (state.uvShieldAvailable) {
				Log(EngineLogLevel::Info, "UV shield hooks installed. Press V to toggle.");
			}
			else {
				Log(EngineLogLevel::Warning, "UV shield hooks are unavailable for this game build.");
			}
		}
		else {
			Log(EngineLogLevel::Warning, "Entity API does not expose UV shield control.");
		}

		if (hostApi->structSize >= DLMOD_HOST_API_V1_HOTKEY_SIZE &&
			hostApi->hotkey != nullptr &&
			hostApi->hotkey->structSize >= DLMOD_HOTKEY_API_V1_SIZE &&
			hostApi->hotkey->abiVersion == DLMOD_HOTKEY_ABI_VERSION &&
			hostApi->hotkey->registerHotkey != nullptr) {
			DlModHotkeyDescV1 hotkey{};
			hotkey.structSize = sizeof(hotkey);
			hotkey.abiVersion = DLMOD_HOTKEY_ABI_VERSION;
			hotkey.id = "example.mod.toggle_uv_shield";
			hotkey.displayName = "Toggle UV Shield";
			hotkey.pageTitle = "Example Mod";
			hotkey.defaultKey = DLMOD_HOTKEY_KEY_F7;
			hotkey.device = DLMOD_HOTKEY_DEVICE_INFER;
			hotkey.userData = &state;
			hotkey.callback = &OnShieldHotkey;
			if (hostApi->hotkey->registerHotkey(&hotkey) == DLMOD_RESULT_SUCCESS) {
				state.hotkey = hostApi->hotkey;
				state.shieldHotkeyRegistered = true;
				Log(EngineLogLevel::Info,
					"UV shield hotkey registered in the native input settings (F7 default).");
			}
			else {
				Log(EngineLogLevel::Warning, "UV shield hotkey registration failed.");
			}
		}
		else {
			Log(EngineLogLevel::Warning, "Hotkey API is unavailable; UV shield has no hotkey.");
		}
		return DLMOD_RESULT_SUCCESS;
	}

	void MODSDK_CALL OnUnload(void* modContext)
	{
		if (modContext != &g_exampleModState) {
			return;
		}

		ExampleModState& state = g_exampleModState;
		if (state.hotkey != nullptr && state.shieldHotkeyRegistered) {
			state.hotkey->unregisterHotkey("example.mod.toggle_uv_shield");
		}
		if (state.entity != nullptr && state.uvShieldAvailable) {
			state.entity->setHunterUvShield(DLMOD_HUNTER_UV_SHIELD_MODE_GAME, 0.0F);
			state.entity->setHunterUvShieldCharges(-1);
			state.entity->setHunterUvShieldDuration(0.0F);
			state.entity->setHunterUvShieldProgress(-1.0F);
		}

		if (state.hostLog != nullptr) {
			state.hostLog(
				state.hostContext,
				static_cast<uint32_t>(EngineLogLevel::Info),
				"Example Mod unloaded.");
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
