#pragma once

#include "sdk/EngineCameraAbi.h"
#include "sdk/EngineCollisionAbi.h"
#include "sdk/EngineEntityAbi.h"
#include "sdk/EngineHookAbi.h"
#include "sdk/EngineHotkeyAbi.h"
#include "sdk/EngineLobbyAbi.h"
#include "sdk/EngineProjectileAbi.h"
#include "sdk/EngineUiInputAbi.h"

/* Host ABI 概览：定义引擎提供给模组的上下文、日志和可选能力函数表。 */
#ifdef __cplusplus
extern "C"
{
#endif

	struct DlModHostApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		void *context;
		EngineLogCallback log;
		/* 旧版本 Host 未提供时为空。 */
		ModEngineGetUiMouseStateFn getUiMouseState;
		/* 游戏碰撞 API 不可用时为空。 */
		const DlModCollisionApiV1 *collision;
		/* 游戏实体 API 不可用时为空。 */
		const DlModEntityApiV1 *entity;
		/* 共享 Hook 注册表不可用时为空。 */
		const DlModHookApiV1 *hooks;
		/* 追加字段以保持旧 Host 布局兼容。 */
		const DlModLobbyApiV1 *lobby;
		/* 弹丸管理器 API 不可用时为空。 */
		const DlModProjectileManagerApiV1 *projectile;
		/* 活动相机 API 不可用时为空。 */
		const DlModCameraApiV1 *camera;
		/* 游戏自带按键绑定系统不可用时为空。 */
		const DlModHotkeyApiV1 *hotkey;
	};

#define DLMOD_HOST_API_V1_MOUSE_STATE_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, getUiMouseState) + sizeof(ModEngineGetUiMouseStateFn)))
#define DLMOD_HOST_API_V1_COLLISION_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, collision) + sizeof(const DlModCollisionApiV1 *)))
#define DLMOD_HOST_API_V1_LOBBY_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, lobby) + sizeof(const DlModLobbyApiV1 *)))
#define DLMOD_HOST_API_V1_ENTITY_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, entity) + sizeof(const DlModEntityApiV1 *)))
#define DLMOD_HOST_API_V1_HOOKS_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, hooks) + sizeof(const DlModHookApiV1 *)))
#define DLMOD_HOST_API_V1_PROJECTILE_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, projectile) + sizeof(const DlModProjectileManagerApiV1 *)))
#define DLMOD_HOST_API_V1_CAMERA_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, camera) + sizeof(const DlModCameraApiV1 *)))
#define DLMOD_HOST_API_V1_HOTKEY_SIZE \
	((uint32_t)(offsetof(DlModHostApiV1, hotkey) + sizeof(const DlModHotkeyApiV1 *)))

#ifdef __cplusplus
}
#endif
