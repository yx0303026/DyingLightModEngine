#include "runtime/EngineLobbyData.h"

#include <algorithm>
#include <cstring>
#include <mutex>

namespace
{
	constexpr std::size_t kMaximumRooms = 200;

}

EngineLobbyData& EngineLobbyData::Instance() noexcept
{
	static EngineLobbyData instance;
	return instance;
}

void EngineLobbyData::ResetLobby(const char* lobbyId) noexcept
{
	const std::unique_lock<std::shared_mutex> lock(mutex_);
	const std::string nextId = lobbyId == nullptr ? std::string{} : std::string(lobbyId);
	if (lobbyId_ == nextId)
		return;
	lobbyId_ = nextId;
	ownerName_.clear();
	ownerSteamId_.clear();
	reportedPlayerCount_ = 0;
	current_.clear();
	status_ = DLMOD_LOBBY_STATUS_WAITING;
	++sequence_;
}

void EngineLobbyData::SetLobbyMetadata(
	const char* lobbyId,
	const char* ownerName,
	const char* ownerSteamId,
	std::uint32_t reportedPlayerCount) noexcept
{
	const std::unique_lock<std::shared_mutex> lock(mutex_);
	if (lobbyId != nullptr && lobbyId[0] != '\0' && lobbyId_ != lobbyId)
	{
		lobbyId_ = lobbyId;
		current_.clear();
	}
	if (ownerName != nullptr && ownerName[0] != '\0')
		ownerName_ = ownerName;
	if (ownerSteamId != nullptr && ownerSteamId[0] != '\0')
		ownerSteamId_ = ownerSteamId;
	if (reportedPlayerCount > 0)
		reportedPlayerCount_ = reportedPlayerCount;
	++sequence_;
}

void EngineLobbyData::ReplaceCurrent(const std::vector<EngineLobbyMemberRecord>& members) noexcept
{
	const std::unique_lock<std::shared_mutex> lock(mutex_);
	current_ = members;
	status_ = current_.empty() ? DLMOD_LOBBY_STATUS_WAITING : DLMOD_LOBBY_STATUS_READY;
	++sequence_;
}

void EngineLobbyData::MergeHistory(const std::vector<EngineLobbyMemberRecord>& members) noexcept
{
	const std::unique_lock<std::shared_mutex> lock(mutex_);
	for (const EngineLobbyMemberRecord& source : members)
	{
		if (source.puid.empty())
			continue;
		EngineLobbyMemberRecord* destination = nullptr;
		for (EngineLobbyMemberRecord& item : history_)
		{
			if (item.puid == source.puid)
			{
				destination = &item;
				break;
			}
		}
		if (destination == nullptr)
		{
			destination = &history_.emplace_back(source);
			destination->hits = std::max<std::uint32_t>(destination->hits, 1);
			continue;
		}
		if (!source.name.empty())
			destination->name = source.name;
		if (!source.steamId.empty())
			destination->steamId = source.steamId;
		if (!source.team.empty())
			destination->team = source.team;
		destination->zombie = destination->zombie || source.zombie;
		destination->hits += std::max<std::uint32_t>(source.hits, 1);
	}
	++sequence_;
}

void EngineLobbyData::AddRoom(const EngineLobbyRoomRecord& room) noexcept
{
	if (room.lobbyId.empty())
		return;
	const std::unique_lock<std::shared_mutex> lock(mutex_);
	for (EngineLobbyRoomRecord& item : rooms_)
	{
		if (item.lobbyId == room.lobbyId)
		{
			if (!room.ownerName.empty())
				item.ownerName = room.ownerName;
			if (!room.ownerSteamId.empty())
				item.ownerSteamId = room.ownerSteamId;
			++sequence_;
			return;
		}
	}
	if (rooms_.size() >= kMaximumRooms)
		rooms_.erase(rooms_.begin());
	rooms_.push_back(room);
	++sequence_;
}

void EngineLobbyData::SetStatus(std::uint32_t status) noexcept
{
	const std::unique_lock<std::shared_mutex> lock(mutex_);
	status_ = status;
	++sequence_;
}

void EngineLobbyData::ClearHistory() noexcept
{
	const std::unique_lock<std::shared_mutex> lock(mutex_);
	history_.clear();
	++sequence_;
}

void EngineLobbyData::CopyText(
	char* destination,
	std::size_t capacity,
	const std::string& value) noexcept
{
	if (destination == nullptr || capacity == 0)
		return;
	std::memset(destination, 0, capacity);
	if (!value.empty())
		strncpy_s(destination, capacity, value.c_str(), capacity - 1);
}

bool EngineLobbyData::CopyMember(
	DlModLobbyMemberV1& destination,
	const EngineLobbyMemberRecord& source) noexcept
{
	std::memset(&destination, 0, sizeof(destination));
	destination.structSize = sizeof(destination);
	destination.flags = source.zombie ? DLMOD_LOBBY_MEMBER_ZOMBIE : 0U;
	destination.hits = source.hits;
	CopyText(destination.puid, sizeof(destination.puid), source.puid);
	CopyText(destination.name, sizeof(destination.name), source.name);
	CopyText(destination.steamId, sizeof(destination.steamId), source.steamId);
	CopyText(destination.team, sizeof(destination.team), source.team);
	return true;
}

bool EngineLobbyData::CopyRoom(
	DlModLobbyRoomV1& destination,
	const EngineLobbyRoomRecord& source) noexcept
{
	std::memset(&destination, 0, sizeof(destination));
	destination.structSize = sizeof(destination);
	CopyText(destination.lobbyId, sizeof(destination.lobbyId), source.lobbyId);
	CopyText(destination.ownerName, sizeof(destination.ownerName), source.ownerName);
	CopyText(destination.ownerSteamId, sizeof(destination.ownerSteamId), source.ownerSteamId);
	return true;
}

DlModResult EngineLobbyData::Capture(DlModLobbySnapshotV1* snapshot) const noexcept
{
	if (snapshot == nullptr || snapshot->structSize < DLMOD_LOBBY_SNAPSHOT_V1_BASE_SIZE ||
		snapshot->abiVersion != MODSDK_ABI_VERSION)
		return DLMOD_RESULT_INVALID_ARGUMENT;

	const std::unique_lock<std::shared_mutex> lock(mutex_);
	DlModLobbySnapshotV1 result{};
	result.structSize = sizeof(result);
	result.abiVersion = MODSDK_ABI_VERSION;
	result.status = status_;
	result.sequence = sequence_;
	result.reportedPlayerCount = reportedPlayerCount_;
	result.currentCount = static_cast<std::uint32_t>(current_.size());
	result.historyCount = static_cast<std::uint32_t>(history_.size());
	result.roomCount = static_cast<std::uint32_t>(rooms_.size());
	result.requiredCurrentCount = result.currentCount;
	result.requiredHistoryCount = result.historyCount;
	result.requiredRoomCount = result.roomCount;
	result.currentCapacity = snapshot->currentCapacity;
	result.historyCapacity = snapshot->historyCapacity;
	result.roomCapacity = snapshot->roomCapacity;
	result.currentMembers = snapshot->currentMembers;
	result.historyMembers = snapshot->historyMembers;
	result.rooms = snapshot->rooms;
	CopyText(result.lobbyId, sizeof(result.lobbyId), lobbyId_);
	CopyText(result.ownerName, sizeof(result.ownerName), ownerName_);
	CopyText(result.ownerSteamId, sizeof(result.ownerSteamId), ownerSteamId_);

	if (result.currentCapacity > 0 && result.currentMembers == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	if (result.historyCapacity > 0 && result.historyMembers == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	if (result.roomCapacity > 0 && result.rooms == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;

	const std::uint32_t currentCount =
		(std::min)(result.currentCapacity, result.requiredCurrentCount);
	const std::uint32_t historyCount =
		(std::min)(result.historyCapacity, result.requiredHistoryCount);
	const std::uint32_t roomCount =
		(std::min)(result.roomCapacity, result.requiredRoomCount);
	result.currentCount = currentCount;
	result.historyCount = historyCount;
	result.roomCount = roomCount;
	if (currentCount < result.requiredCurrentCount)
		result.truncatedFlags |= DLMOD_LOBBY_SNAPSHOT_TRUNCATED_CURRENT;
	if (historyCount < result.requiredHistoryCount)
		result.truncatedFlags |= DLMOD_LOBBY_SNAPSHOT_TRUNCATED_HISTORY;
	if (roomCount < result.requiredRoomCount)
		result.truncatedFlags |= DLMOD_LOBBY_SNAPSHOT_TRUNCATED_ROOMS;
	for (std::uint32_t index = 0; index < currentCount; ++index)
		CopyMember(result.currentMembers[index], current_[index]);
	for (std::uint32_t index = 0; index < historyCount; ++index)
		CopyMember(result.historyMembers[index], history_[index]);
	for (std::uint32_t index = 0; index < roomCount; ++index)
		CopyRoom(result.rooms[index], rooms_[index]);

	const std::size_t copySize =
		(std::min)(static_cast<std::size_t>(snapshot->structSize), sizeof(result));
	std::memcpy(snapshot, &result, copySize);
	return result.truncatedFlags == 0U ? DLMOD_RESULT_SUCCESS : DLMOD_RESULT_PARTIAL_SUCCESS;
}

