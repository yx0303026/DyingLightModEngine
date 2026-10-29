#pragma once

#include "sdk/EngineLobbyAbi.h"

#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace cheat_mod
{
	// 负责读取 Engine Lobby ABI，并维护大厅快照与成员到场记录。
	class CheatModLobby final
	{
	public:
		CheatModLobby() = default;
		~CheatModLobby() = default;
		CheatModLobby(const CheatModLobby&) = delete;
		CheatModLobby& operator=(const CheatModLobby&) = delete;

		void SetApi(const DlModLobbyApiV1* api) noexcept;
		void Refresh() noexcept;
		void ClearHistory() noexcept;
		void CopyCurrentToClipboard() const noexcept;

		const std::string& GetStatusText() const noexcept;
		const std::vector<DlModLobbyMemberV1>& GetCurrentMembers() const noexcept;
		const std::vector<DlModLobbyMemberV1>& GetHistoryMembers() const noexcept;
		const std::vector<DlModLobbyRoomV1>& GetRooms() const noexcept;
		std::uint32_t GetCurrentCount() const noexcept;
		std::uint32_t GetHistoryCount() const noexcept;
		std::uint32_t GetRoomCount() const noexcept;
		static const char* GetTeamText(const DlModLobbyMemberV1& member) noexcept;

	private:
		static bool CompareRecordFiles(
			const std::pair<FILETIME, std::string>& left,
			const std::pair<FILETIME, std::string>& right) noexcept;
		void SetUnavailable() noexcept;
		void OpenSessionFile() noexcept;
		void RecordNewMembers(const DlModLobbySnapshotV1& snapshot) noexcept;
		void AppendMemberRecord(const DlModLobbyMemberV1& member) const noexcept;
		void TrimRecordDirectory() const noexcept;
		bool IsNewArrival(const char* puid) const noexcept;

		const DlModLobbyApiV1* api_ = nullptr;
		std::vector<DlModLobbyMemberV1> currentMembers_;
		std::vector<DlModLobbyMemberV1> historyMembers_;
		std::vector<DlModLobbyRoomV1> rooms_;
		std::string statusText_ = "大厅数据不可用";
		std::uint32_t currentCount_ = 0;
		std::uint32_t historyCount_ = 0;
		std::uint32_t roomCount_ = 0;
		std::string sessionFile_;
		std::string lastLobbyId_;
		std::vector<std::string> previousPuids_;
		bool sessionOpened_ = false;
	};
}
