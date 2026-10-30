#pragma once

#include "sdk/EngineHotkeyAbi.h"

#include <cstdint>
#include <string>

/*
 * Hotkey runtime 概览：
 * - 把模组注册的热键注入游戏原生 GameInputsConverter。
 * - 复用游戏按键绑定页面与改键逻辑，不重写菜单。
 * - 通过 GetAsyncKeyState + 当前绑定键轮询触发回调。
 */
class EngineHotkeyRuntime final {
public:
	EngineHotkeyRuntime() = delete;

	/* 在模组全部加载后调用：安装输入脚本刷新 hook。 */
	static void Initialize() noexcept;

	/* 每帧调用（渲染线程）：注入待处理注册并轮询按键。 */
	static void Tick() noexcept;

	static void Shutdown() noexcept;

	static const DlModHotkeyApiV1* GetApi() noexcept;

	/* 模组卸载/加载失败时清理其注册，避免回调指向已卸载 DLL。 */
	static void RemoveForOwner(const std::string& owner) noexcept;

private:
	static DlModResult MODSDK_CALL RegisterHotkey(
		const DlModHotkeyDescV1* desc) noexcept;
	static uint32_t MODSDK_CALL GetHotkeyKey(const char* id) noexcept;
	static uint32_t MODSDK_CALL GetHotkeyAction(const char* id) noexcept;
	static DlModResult MODSDK_CALL UnregisterHotkey(const char* id) noexcept;
};

namespace EngineHotkeyRuntimeDetail {
	/* 与 EngineHookRuntimeDetail::SetLoadingMod 相同：标记当前 onLoad 所属模组。 */
	const std::string* SetLoadingMod(const std::string* modId) noexcept;
}
