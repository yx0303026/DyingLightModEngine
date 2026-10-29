#include "CheatModLobby.h"

#include <cstdio>
#include <string>
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <cstring>

namespace cheat_mod
{
	namespace
	{
		constexpr std::uint32_t kMemberCapacity = 64;
		constexpr std::uint32_t kRoomCapacity = 200;

		const char* SafeText(const char* value, const char* fallback) noexcept
		{
			return value != nullptr && value[0] != '\0' ? value : fallback;
		}

		std::string GetModuleDirectory() noexcept
		{
			HMODULE module = nullptr;
			if (!GetModuleHandleExA(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(&GetModuleDirectory), &module))
				return {};
			char modulePath[MAX_PATH]{};
			if (GetModuleFileNameA(module, modulePath, sizeof(modulePath)) == 0)
				return {};
			char* separator = std::strrchr(modulePath, '\\');
			if (separator != nullptr)
				separator[1] = '\0';
			return modulePath;
		}
	}

	void CheatModLobby::SetApi(const DlModLobbyApiV1* api) noexcept
	{
		api_ = api;
		if (api_ == nullptr || api_->capture == nullptr)
			SetUnavailable();
	}

	void CheatModLobby::Refresh() noexcept
	{
		if (api_ == nullptr || api_->capture == nullptr || api_->controlObject == nullptr)
		{
			SetUnavailable();
			return;
		}
		try
		{
			currentMembers_.resize(kMemberCapacity);
			historyMembers_.resize(kMemberCapacity);
			rooms_.resize(kRoomCapacity);
			DlModLobbySnapshotV1 snapshot{};
			snapshot.structSize = sizeof(snapshot);
			snapshot.abiVersion = MODSDK_ABI_VERSION;
			snapshot.currentCapacity = static_cast<std::uint32_t>(currentMembers_.size());
			snapshot.historyCapacity = static_cast<std::uint32_t>(historyMembers_.size());
			snapshot.roomCapacity = static_cast<std::uint32_t>(rooms_.size());
			snapshot.currentMembers = currentMembers_.data();
			snapshot.historyMembers = historyMembers_.data();
			snapshot.rooms = rooms_.data();
			const DlModResult result = api_->capture(api_->controlObject, &snapshot);
			if (result != DLMOD_RESULT_SUCCESS && result != DLMOD_RESULT_PARTIAL_SUCCESS)
			{
				SetUnavailable();
				return;
			}
			currentCount_ = snapshot.currentCount;
			historyCount_ = snapshot.historyCount;
			roomCount_ = snapshot.roomCount;
			currentText_ = FormatMembers(currentMembers_.data(), currentCount_);
			historyText_ = FormatMembers(historyMembers_.data(), historyCount_);
			roomsText_ = FormatRooms(rooms_.data(), roomCount_);
			RecordNewMembers(snapshot);
			char status[256]{};
			const char* lobbyId = SafeText(snapshot.lobbyId, "未加入房间");
			const char* ownerName = SafeText(snapshot.ownerName, "未知");
			const char* ownerSteamId = SafeText(snapshot.ownerSteamId, "未知");
			const char* state = snapshot.status == DLMOD_LOBBY_STATUS_READY ? "已同步" :
				snapshot.status == DLMOD_LOBBY_STATUS_STALE ? "等待成员数据" : "等待进房";
			_snprintf_s(status, sizeof(status), _TRUNCATE,
				"状态：%s\n房间：%s\n房主：%s\n房主 SteamID：%s\n人数：%u",
				state, lobbyId, ownerName, ownerSteamId,
				snapshot.reportedPlayerCount > 0 ? snapshot.reportedPlayerCount : snapshot.currentCount);
			statusText_ = status;
		}
		catch (...)
		{
			SetUnavailable();
		}
	}

	void CheatModLobby::ClearHistory() noexcept
	{
		if (api_ != nullptr && api_->clearHistory != nullptr && api_->controlObject != nullptr)
			api_->clearHistory(api_->controlObject);
		historyCount_ = 0;
		historyText_ = "（暂无历史成员）";
	}

	void CheatModLobby::CopyCurrentToClipboard() const noexcept
	{
		if (!OpenClipboard(nullptr))
			return;
		std::string text = "Name\\tPUID\\tTeam\\tSteamID\\r\\n";
		for (std::uint32_t index = 0; index < currentCount_; ++index)
		{
			const DlModLobbyMemberV1& member = currentMembers_[index];
			text += SafeText(member.name, "Unknown");
			text += '\t';
			text += SafeText(member.puid, "Unknown");
			text += '\t';
			text += GetTeamText(member);
			text += '\t';
			text += SafeText(member.steamId, "Unreported");
			text += "\\r\\n";
		}
		EmptyClipboard();
		HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
		if (memory != nullptr)
		{
			void* buffer = GlobalLock(memory);
			if (buffer != nullptr)
			{
				std::memcpy(buffer, text.c_str(), text.size() + 1);
				GlobalUnlock(memory);
				SetClipboardData(CF_TEXT, memory);
			}
			else
				GlobalFree(memory);
		}
		CloseClipboard();
	}

	const std::string& CheatModLobby::GetStatusText() const noexcept { return statusText_; }
	const std::string& CheatModLobby::GetCurrentText() const noexcept { return currentText_; }
	const std::string& CheatModLobby::GetHistoryText() const noexcept { return historyText_; }
	const std::string& CheatModLobby::GetRoomsText() const noexcept { return roomsText_; }
	std::uint32_t CheatModLobby::GetCurrentCount() const noexcept { return currentCount_; }
	std::uint32_t CheatModLobby::GetHistoryCount() const noexcept { return historyCount_; }
	std::uint32_t CheatModLobby::GetRoomCount() const noexcept { return roomCount_; }

	std::string CheatModLobby::FormatMember(const DlModLobbyMemberV1& member)
	{
		char line[384]{};
		_snprintf_s(line, sizeof(line), _TRUNCATE,
			"%s | PUID：%s | 阵营：%s | SteamID：%s | 命中：%u",
			SafeText(member.name, "未知"), SafeText(member.puid, "未知"),
			GetTeamText(member), SafeText(member.steamId, "未上报"), member.hits);
		return line;
	}

	std::string CheatModLobby::FormatMembers(const DlModLobbyMemberV1* members, std::uint32_t count)
	{
		if (members == nullptr || count == 0)
			return "（暂无成员数据）";
		std::string result;
		for (std::uint32_t index = 0; index < count; ++index)
		{
			if (!result.empty())
				result.push_back('\n');
			result += FormatMember(members[index]);
		}
		return result;
	}

	std::string CheatModLobby::FormatRooms(const DlModLobbyRoomV1* rooms, std::uint32_t count)
	{
		if (rooms == nullptr || count == 0)
			return "（暂无房间搜索结果）";
		std::string result;
		for (std::uint32_t index = 0; index < count; ++index)
		{
			char line[256]{};
			_snprintf_s(line, sizeof(line), _TRUNCATE,
				"%s | 房号：%s | 房主 SteamID：%s",
				SafeText(rooms[index].ownerName, "未知"),
				SafeText(rooms[index].lobbyId, "未知"),
				SafeText(rooms[index].ownerSteamId, "未上报"));
			if (!result.empty())
				result.push_back('\n');
			result += line;
		}
		return result;
	}

	const char* CheatModLobby::GetTeamText(const DlModLobbyMemberV1& member) noexcept
	{
		if ((member.flags & DLMOD_LOBBY_MEMBER_ZOMBIE) != 0U)
			return "猎手";
		if (member.team[0] != '\0' && std::string(member.team) != "0")
			return "人类";
		return "未知";
	}

	void CheatModLobby::SetUnavailable() noexcept
	{
		currentCount_ = 0;
		historyCount_ = 0;
		roomCount_ = 0;
		statusText_ = "大厅数据不可用";
		currentText_ = "（暂无成员数据）";
		historyText_ = "（暂无历史成员）";
		roomsText_ = "（暂无房间搜索结果）";
	}

	void CheatModLobby::OpenSessionFile() noexcept
	{
		const std::string directory = GetModuleDirectory() + "记录\\";
		if (directory.size() <= 3)
			return;
		CreateDirectoryA(directory.c_str(), nullptr);
		SYSTEMTIME time{};
		GetLocalTime(&time);
		char filePath[MAX_PATH]{};
		_snprintf_s(filePath, sizeof(filePath), _TRUNCATE,
			"%s%04u-%02u-%02u_%02u-%02u-%02u.txt", directory.c_str(),
			time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
		sessionFile_ = filePath;
		std::ofstream file(sessionFile_, std::ios::app);
		if (file)
			file << "===== Room List session =====\\r\\n";
		sessionOpened_ = true;
	}

	void CheatModLobby::RecordNewMembers(const DlModLobbySnapshotV1& snapshot) noexcept
	{
		if (snapshot.lobbyId[0] != '\0' && lastLobbyId_ != snapshot.lobbyId)
		{
			lastLobbyId_ = snapshot.lobbyId;
			previousPuids_.clear();
			sessionOpened_ = false;
		}
		if (!sessionOpened_)
			OpenSessionFile();
		for (std::uint32_t index = 0; index < currentCount_; ++index)
		{
			const DlModLobbyMemberV1& member = currentMembers_[index];
			if (member.puid[0] != '\0' && IsNewArrival(member.puid))
				AppendMemberRecord(member);
		}
		previousPuids_.clear();
		for (std::uint32_t index = 0; index < currentCount_; ++index)
			previousPuids_.emplace_back(currentMembers_[index].puid);
		TrimRecordDirectory();
	}

	void CheatModLobby::AppendMemberRecord(const DlModLobbyMemberV1& member) const noexcept
	{
		if (sessionFile_.empty())
			return;
		std::ofstream file(sessionFile_, std::ios::app);
		if (!file)
			return;
		SYSTEMTIME time{};
		GetLocalTime(&time);
		file << '[' << time.wHour << ':' << time.wMinute << ':' << time.wSecond << "] "
			<< SafeText(member.name, "Unknown") << " | " << SafeText(member.puid, "Unknown")
			<< " | " << SafeText(member.steamId, "Unreported") << " | " << GetTeamText(member) << "\\r\\n";
	}

	void CheatModLobby::TrimRecordDirectory() const noexcept
	{
		const std::string directory = GetModuleDirectory() + "记录\\";
		if (directory.size() <= 3)
			return;
		WIN32_FIND_DATAA data{};
		const std::string pattern = directory + "*.txt";
		HANDLE search = FindFirstFileA(pattern.c_str(), &data);
		if (search == INVALID_HANDLE_VALUE)
			return;
		std::vector<std::pair<FILETIME, std::string>> files;
		do
		{
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				files.emplace_back(data.ftLastWriteTime, data.cFileName);
		} while (FindNextFileA(search, &data));
		FindClose(search);
		std::sort(files.begin(), files.end(), &CheatModLobby::CompareRecordFiles);
		while (files.size() > 10)
		{
			DeleteFileA((directory + files.front().second).c_str());
			files.erase(files.begin());
		}
	}

	bool CheatModLobby::IsNewArrival(const char* puid) const noexcept
	{
		return std::find(previousPuids_.begin(), previousPuids_.end(), puid) == previousPuids_.end();
	}

	bool CheatModLobby::CompareRecordFiles(
		const std::pair<FILETIME, std::string>& left,
		const std::pair<FILETIME, std::string>& right) noexcept
	{
		return CompareFileTime(&left.first, &right.first) < 0;
	}
}
