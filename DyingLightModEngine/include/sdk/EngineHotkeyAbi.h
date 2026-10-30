#pragma once

#include "sdk/EngineAbiTypes.h"

/*
 * Hotkey ABI 概览：把模组热键注册进游戏自带的按键绑定系统。
 *
 * 设计约束（来自 gamedll_x64_rwdi.dll 逆向）：
 * - 游戏的输入动作槽固定为 0..337（共 338 个，其中包含未命名空槽）。
 * - 页面（View）与可改键行（LayoutKeybinding）由原生 GameInputsConverter 持有。
 * - 模组热键会在下一次输入脚本解析时注入；UI 与改键沿用游戏原逻辑。
 *
 * 注册时机：请在模组 OnLoad 中调用。若在输入系统初始化完成之后才注册，
 * 运行时会在下一次输入刷新时尝试补齐。
 */
#ifdef __cplusplus
extern "C"
{
#endif

#define DLMOD_HOTKEY_ABI_VERSION 1u

	/* 动作 ID 无效值。 */
#define DLMOD_HOTKEY_INVALID_ACTION 0xFFFFFFFFu
	/* 尚未解析到当前绑定键时的返回值。 */
#define DLMOD_HOTKEY_INVALID_KEY 0xFFFFFFFFu

	/* 设备字段：0 表示根据按键编码自动推断（推荐）。 */
#define DLMOD_HOTKEY_DEVICE_INFER 0u

	/* 常用 DirectInput 扫描码，便于模组直接使用。 */
#define DLMOD_HOTKEY_KEY_ESCAPE 0x01u
#define DLMOD_HOTKEY_KEY_1 0x02u
#define DLMOD_HOTKEY_KEY_2 0x03u
#define DLMOD_HOTKEY_KEY_3 0x04u
#define DLMOD_HOTKEY_KEY_4 0x05u
#define DLMOD_HOTKEY_KEY_5 0x06u
#define DLMOD_HOTKEY_KEY_6 0x07u
#define DLMOD_HOTKEY_KEY_7 0x08u
#define DLMOD_HOTKEY_KEY_8 0x09u
#define DLMOD_HOTKEY_KEY_9 0x0Au
#define DLMOD_HOTKEY_KEY_0 0x0Bu
#define DLMOD_HOTKEY_KEY_TAB 0x0Fu
#define DLMOD_HOTKEY_KEY_Q 0x10u
#define DLMOD_HOTKEY_KEY_W 0x11u
#define DLMOD_HOTKEY_KEY_E 0x12u
#define DLMOD_HOTKEY_KEY_R 0x13u
#define DLMOD_HOTKEY_KEY_T 0x14u
#define DLMOD_HOTKEY_KEY_Y 0x15u
#define DLMOD_HOTKEY_KEY_U 0x16u
#define DLMOD_HOTKEY_KEY_I 0x17u
#define DLMOD_HOTKEY_KEY_O 0x18u
#define DLMOD_HOTKEY_KEY_P 0x19u
#define DLMOD_HOTKEY_KEY_A 0x1Eu
#define DLMOD_HOTKEY_KEY_S 0x1Fu
#define DLMOD_HOTKEY_KEY_D 0x20u
#define DLMOD_HOTKEY_KEY_F 0x21u
#define DLMOD_HOTKEY_KEY_G 0x22u
#define DLMOD_HOTKEY_KEY_H 0x23u
#define DLMOD_HOTKEY_KEY_J 0x24u
#define DLMOD_HOTKEY_KEY_K 0x25u
#define DLMOD_HOTKEY_KEY_L 0x26u
#define DLMOD_HOTKEY_KEY_Z 0x2Cu
#define DLMOD_HOTKEY_KEY_X 0x2Du
#define DLMOD_HOTKEY_KEY_C 0x2Eu
#define DLMOD_HOTKEY_KEY_V 0x2Fu
#define DLMOD_HOTKEY_KEY_B 0x30u
#define DLMOD_HOTKEY_KEY_N 0x31u
#define DLMOD_HOTKEY_KEY_M 0x32u
#define DLMOD_HOTKEY_KEY_F1 0x3Bu
#define DLMOD_HOTKEY_KEY_F2 0x3Cu
#define DLMOD_HOTKEY_KEY_F3 0x3Du
#define DLMOD_HOTKEY_KEY_F4 0x3Eu
#define DLMOD_HOTKEY_KEY_F5 0x3Fu
#define DLMOD_HOTKEY_KEY_F6 0x40u
#define DLMOD_HOTKEY_KEY_F7 0x41u
#define DLMOD_HOTKEY_KEY_F8 0x42u
#define DLMOD_HOTKEY_KEY_F9 0x43u
#define DLMOD_HOTKEY_KEY_F10 0x44u
#define DLMOD_HOTKEY_KEY_F11 0x57u
#define DLMOD_HOTKEY_KEY_F12 0x58u
#define DLMOD_HOTKEY_KEY_LSHIFT 0x2Au
#define DLMOD_HOTKEY_KEY_RSHIFT 0x36u
#define DLMOD_HOTKEY_KEY_LCONTROL 0x1Du
#define DLMOD_HOTKEY_KEY_RCONTROL 0x9Du
#define DLMOD_HOTKEY_KEY_LALT 0x38u
#define DLMOD_HOTKEY_KEY_RALT 0xB8u
#define DLMOD_HOTKEY_KEY_SPACE 0x39u
#define DLMOD_HOTKEY_KEY_ENTER 0x1Cu
#define DLMOD_HOTKEY_KEY_BACKSPACE 0x0Eu
#define DLMOD_HOTKEY_KEY_MOUSE_LEFT 0xF000u
#define DLMOD_HOTKEY_KEY_MOUSE_RIGHT 0xF001u
#define DLMOD_HOTKEY_KEY_MOUSE_MIDDLE 0xF002u
#define DLMOD_HOTKEY_KEY_MOUSE_X1 0xF003u
#define DLMOD_HOTKEY_KEY_MOUSE_X2 0xF004u

	/* 按下沿触发；userData 与注册时一致，actionId 由引擎分配。 */
	typedef void(MODSDK_CALL *DlModHotkeyCallbackV1)(
		void *userData,
		uint32_t actionId);

	typedef struct DlModHotkeyDescV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		/* 模组内唯一标识（建议 "modid.feature"）。 */
		const char *id;
		/* 按键绑定行显示文本（UTF-8 或 ASCII；纯文本即可）。 */
		const char *displayName;
		/* 页面标题；nullptr 或空串表示放入默认页 "Mods"。 */
		const char *pageTitle;
		/* 默认按键编码（DirectInput 扫描码 / 鼠标 0xF000+）。 */
		uint32_t defaultKey;
		/* 保留：DLMOD_HOTKEY_DEVICE_INFER 表示按键编码自动推断。 */
		uint32_t device;
		/* 保留：写入绑定记录的目标（EInputTarget_*），0 即可。 */
		uint32_t target;
		void *userData;
		DlModHotkeyCallbackV1 callback;
	} DlModHotkeyDescV1;

	typedef DlModResult(MODSDK_CALL *DlModHotkeyRegisterFn)(
		const DlModHotkeyDescV1 *desc);

	/* 返回当前实际绑定键；未注入/未知返回 DLMOD_HOTKEY_INVALID_KEY。 */
	typedef uint32_t(MODSDK_CALL *DlModHotkeyGetKeyFn)(
		const char *id);

	/* 返回分配到的动作 ID；未知返回 DLMOD_HOTKEY_INVALID_ACTION。 */
	typedef uint32_t(MODSDK_CALL *DlModHotkeyGetActionFn)(
		const char *id);

	/* 移除注册；模组 OnUnload 中必须调用，避免回调指向已卸载的 DLL。 */
	typedef DlModResult(MODSDK_CALL *DlModHotkeyUnregisterFn)(
		const char *id);

	typedef struct DlModHotkeyApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		DlModHotkeyRegisterFn registerHotkey;
		DlModHotkeyGetKeyFn getHotkeyKey;
		DlModHotkeyGetActionFn getHotkeyAction;
		DlModHotkeyUnregisterFn unregisterHotkey;
	} DlModHotkeyApiV1;

#define DLMOD_HOTKEY_API_V1_SIZE \
	((uint32_t)(offsetof(DlModHotkeyApiV1, unregisterHotkey) + sizeof(DlModHotkeyUnregisterFn)))

#ifdef __cplusplus
}
#endif
