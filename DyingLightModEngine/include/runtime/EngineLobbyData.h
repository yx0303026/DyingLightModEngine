#pragma once

#include "sdk/EngineLobbyAbi.h"

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <vector>

// 大厅成员的内部快照记录，避免把可变字符串直接暴露给模组。
struct EngineLobbyMemberRecord final
{
	std::string puid;
	std::string name;
	std::string steamId;
	std::string team;
	bool zombie = false;
	std::uint32_t hits = 0;
};

// 搜索结果的内部快照记录。
struct EngineLobbyRoomRecord final
{
	std::string lobbyId;
	std::string ownerName;
	std::string ownerSteamId;
};

// 负责大厅快照的并发存储、去重和 ABI 缓冲区复制。
class EngineLobbyData final
{
public:
	static EngineLobbyData& Instance() noexcept;

	void ResetLobby(const char* lobbyId) noexcept;
	void SetLobbyMetadata(
		const char* lobbyId,
		const char* ownerName,
		const char* ownerSteamId,
		std::uint32_t reportedPlayerCount) noexcept;
	void ReplaceCurrent(const std::vector<EngineLobbyMemberRecord>& members) noexcept;
	void MergeHistory(const std::vector<EngineLobbyMemberRecord>& members) noexcept;
	void AddRoom(const EngineLobbyRoomRecord& room) noexcept;
	void SetStatus(std::uint32_t status) noexcept;
	void ClearHistory() noexcept;

	DlModResult Capture(DlModLobbySnapshotV1* snapshot) const noexcept;

private:
	EngineLobbyData() = default;
	~EngineLobbyData() = default;
	EngineLobbyData(const EngineLobbyData&) = delete;
	EngineLobbyData& operator=(const EngineLobbyData&) = delete;

	static void CopyText(char* destination, std::size_t capacity, const std::string& value) noexcept;
	static bool CopyMember(
		DlModLobbyMemberV1& destination,
		const EngineLobbyMemberRecord& source) noexcept;
	static bool CopyRoom(
		DlModLobbyRoomV1& destination,
		const EngineLobbyRoomRecord& source) noexcept;

	mutable std::shared_mutex mutex_;
	std::string lobbyId_;
	std::string ownerName_;
	std::string ownerSteamId_;
	std::uint32_t reportedPlayerCount_ = 0;
	std::uint32_t status_ = DLMOD_LOBBY_STATUS_WAITING;
	std::uint64_t sequence_ = 0;
	std::vector<EngineLobbyMemberRecord> current_;
	std::vector<EngineLobbyMemberRecord> history_;
	std::vector<EngineLobbyRoomRecord> rooms_;
};

