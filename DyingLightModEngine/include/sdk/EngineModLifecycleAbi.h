#pragma once

#include "sdk/EngineHostAbi.h"

/* 模组生命周期 ABI 概览：定义模组查询、加载、卸载、渲染和窗口消息回调。 */
#ifdef __cplusplus
extern "C"
{
#endif

	typedef DlModResult(MODSDK_CALL *DlModOnLoadFn)(
		void *modContext,
		const DlModHostApiV1 *hostApi);
	typedef void(MODSDK_CALL *DlModOnUnloadFn)(void *modContext);
	typedef void(MODSDK_CALL *DlModOnRenderFrameFn)(
		void *modContext,
		void *graphicsContext);
	typedef uint8_t(MODSDK_CALL *DlModIsUiOpenFn)(void *modContext);
	typedef void(MODSDK_CALL *DlModOnWindowMessageFn)(
		void *modContext,
		uintptr_t window,
		uint32_t message,
		uintptr_t wParam,
		intptr_t lParam);

	/* 注册期间复制字符串；context 在模组卸载前由模组持有。 */
	typedef struct DlModDescriptorV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		const char *id;
		const char *name;
		const char *version;
		void *context;
		DlModOnLoadFn onLoad;
		DlModOnUnloadFn onUnload;
		/* 可选：游戏根目录 Logs 下的目录名。 */
		const char *logName;
		/* 可选：由引擎图形呈现线程调用。 */
		DlModOnRenderFrameFn onRenderFrame;
		/* 可选：引擎输入路由使用的 UI 状态回调。 */
		DlModIsUiOpenFn isUiOpen;
		/* 可选：用于接收 Win32 消息的回调。 */
		DlModOnWindowMessageFn onWindowMessage;
	} DlModDescriptorV1;

#define DLMOD_DESCRIPTOR_V1_MIN_SIZE ((uint32_t)offsetof(DlModDescriptorV1, logName))

	/* 所有模组必须在 ModEngineRegisterMod 调用前完成注册。 */
	typedef DlModResult(MODSDK_CALL *DlModQueryModFn)(
		uint32_t requestedAbiVersion,
		DlModDescriptorV1 *descriptor);

#ifdef __cplusplus
}
#endif
