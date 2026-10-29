#pragma once

#include "sdk/EngineCollisionAbi.h"
#include "sdk/EngineEntityAbi.h"
#include "sdk/EngineHookAbi.h"
#include "sdk/EngineLobbyAbi.h"
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

#ifdef __cplusplus
}
#endif
