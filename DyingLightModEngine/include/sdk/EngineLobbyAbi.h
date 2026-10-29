#pragma once

#include "sdk/EngineAbiTypes.h"

/* 大厅 ABI 概览：定义大厅成员、房间、稳定快照及其查询和清理函数表。 */
#ifdef __cplusplus
extern "C"
{
#endif

#define DLMOD_LOBBY_STATUS_UNAVAILABLE 0u
#define DLMOD_LOBBY_STATUS_WAITING 1u
#define DLMOD_LOBBY_STATUS_READY 2u
#define DLMOD_LOBBY_STATUS_STALE 3u
#define DLMOD_LOBBY_MEMBER_ZOMBIE 0x00000001u
#define DLMOD_LOBBY_SNAPSHOT_TRUNCATED_CURRENT 0x00000001u
#define DLMOD_LOBBY_SNAPSHOT_TRUNCATED_HISTORY 0x00000002u
#define DLMOD_LOBBY_SNAPSHOT_TRUNCATED_ROOMS 0x00000004u
	typedef struct DlModLobbyMemberV1
	{
		uint32_t structSize;
		uint32_t flags;
		uint32_t hits;
		char puid[64];
		char name[96];
		char steamId[48];
		char team[16];
	} DlModLobbyMemberV1;

	typedef struct DlModLobbyRoomV1
	{
		uint32_t structSize;
		char lobbyId[64];
		char ownerName[96];
		char ownerSteamId[48];
	} DlModLobbyRoomV1;

	typedef struct DlModLobbySnapshotV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t status;
		uint32_t truncatedFlags;
		uint64_t sequence;
		uint32_t reportedPlayerCount;
		uint32_t currentCount;
		uint32_t historyCount;
		uint32_t roomCount;
		uint32_t requiredCurrentCount;
		uint32_t requiredHistoryCount;
		uint32_t requiredRoomCount;
		char lobbyId[64];
		char ownerName[96];
		char ownerSteamId[48];
		uint32_t currentCapacity;
		uint32_t historyCapacity;
		uint32_t roomCapacity;
		DlModLobbyMemberV1 *currentMembers;
		DlModLobbyMemberV1 *historyMembers;
		DlModLobbyRoomV1 *rooms;
	} DlModLobbySnapshotV1;

#define DLMOD_LOBBY_MEMBER_V1_BASE_SIZE ((uint32_t)sizeof(DlModLobbyMemberV1))
#define DLMOD_LOBBY_ROOM_V1_BASE_SIZE ((uint32_t)sizeof(DlModLobbyRoomV1))
#define DLMOD_LOBBY_SNAPSHOT_V1_BASE_SIZE ((uint32_t)offsetof(DlModLobbySnapshotV1, currentMembers))

	typedef DlModResult(MODSDK_CALL *ModEngineCaptureLobbyFn)(
		void *controlObject,
		DlModLobbySnapshotV1 *snapshot);
	typedef DlModResult(MODSDK_CALL *ModEngineClearLobbyHistoryFn)(void *controlObject);

	typedef struct DlModLobbyApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		void *controlObject;
		ModEngineCaptureLobbyFn capture;
		ModEngineClearLobbyHistoryFn clearHistory;
	} DlModLobbyApiV1;

#define DLMOD_LOBBY_API_V1_MIN_SIZE \
	((uint32_t)(offsetof(DlModLobbyApiV1, capture) + sizeof(ModEngineCaptureLobbyFn)))
#define DLMOD_LOBBY_API_V1_HISTORY_SIZE \
	((uint32_t)(offsetof(DlModLobbyApiV1, clearHistory) + sizeof(ModEngineClearLobbyHistoryFn)))

#ifdef __cplusplus
}
#endif
