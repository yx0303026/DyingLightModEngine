#pragma once

#include "sdk/EngineAbiTypes.h"

/* UI 输入 ABI 概览：定义 Host 转发给模组界面的 DirectInput 鼠标状态。 */
#ifdef __cplusplus
extern "C"
{
#endif

#define DLMOD_UI_MOUSE_POSITION_VALID 0x1u
#define DLMOD_UI_MOUSE_DEVICE_CAPTURED 0x2u

	/* UI 开启时从游戏 DirectInput 鼠标设备采样的状态。 */
	typedef struct DlModUiMouseStateV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t flags;
		/* 客户区光标位置；仅在 DLMOD_UI_MOUSE_POSITION_VALID 标志有效时可用。 */
		int32_t positionX;
		int32_t positionY;
		/* 自上次调用以来累计的滚轮刻度。 */
		float wheelDelta;
		/* 按下状态位：bit0 左键、bit1 右键、bit2 中键、bit3 X1、bit4 X2。 */
		uint32_t buttons;
	} DlModUiMouseStateV1;

	/* 返回非零表示 UI 拥有虚拟指针；设备采集标志表示 DirectInput 桥接已启用。 */
	typedef uint8_t(MODSDK_CALL *ModEngineGetUiMouseStateFn)(
		DlModUiMouseStateV1 *state);

#ifdef __cplusplus
}
#endif
