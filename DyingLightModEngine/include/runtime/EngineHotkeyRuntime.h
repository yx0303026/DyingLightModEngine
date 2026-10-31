#pragma once

#include "sdk/EngineHotkeyAbi.h"

#include <cstdint>
#include <string>

/*
 * 热键运行时概览：
 * - 保存模组注册的热键，并注入游戏原生 GameInputsConverter。
 * - 复用游戏按键设置页面和改键逻辑，不创建独立菜单。
 * - 通过 GetAsyncKeyState 和当前绑定键触发模组回调。
 */
class EngineHotkeyRuntime final {
public:
	EngineHotkeyRuntime() = delete;

	/* 在模组全部加载后调用：安装输入刷新钩子并准备原生页面注入。 */
	static void Initialize() noexcept;

	/* 每帧调用（渲染线程）：注入待处理注册并轮询当前绑定键。 */
	static void Tick() noexcept;

	static void Shutdown() noexcept;

	static const DlModHotkeyApiV1* GetApi() noexcept;

	/* 模组卸载或加载失败时清理其注册，避免回调指向已卸载 DLL。 */
	static void RemoveForOwner(const std::string& owner) noexcept;

private:
	static DlModResult MODSDK_CALL RegisterHotkey(
		const DlModHotkeyDescV1* desc) noexcept;
	static uint32_t MODSDK_CALL GetHotkeyKey(const char* id) noexcept;
	static uint32_t MODSDK_CALL GetHotkeyAction(const char* id) noexcept;
	static DlModResult MODSDK_CALL UnregisterHotkey(const char* id) noexcept;
};

namespace EngineHotkeyRuntimeDetail {
	/* 标记当前 onLoad 所属模组，供热键记录关联生命周期。 */
	const std::string* SetLoadingMod(const std::string* modId) noexcept;
}
