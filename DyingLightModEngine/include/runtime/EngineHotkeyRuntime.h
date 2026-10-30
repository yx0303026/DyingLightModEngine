#pragma once

#include "sdk/EngineHotkeyAbi.h"

#include <cstdint>
#include <string>

/*
 * 热键运行时概览：
 * - 保存模组注册的热键，并在渲染线程轮询按键状态。
 * - 使用注册描述中的默认按键，避免修改游戏内部输入容器。
 * - 通过 GetAsyncKeyState 触发模组回调，保证生命周期和分配器边界清晰。
 */
class EngineHotkeyRuntime final {
public:
	EngineHotkeyRuntime() = delete;

	/* 在模组全部加载后调用：初始化安全的热键轮询状态。 */
	static void Initialize() noexcept;

	/* 每帧调用（渲染线程）：准备注册并轮询默认按键。 */
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
