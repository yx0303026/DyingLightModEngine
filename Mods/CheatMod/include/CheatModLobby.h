#pragma once

#include "sdk/EngineLobbyAbi.h"

#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace cheat_mod
{
	// 负责读取 Engine Lobby ABI，并把固定缓冲区转换为页面可显示文本。
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
		const std::string& GetCurrentText() const noexcept;
		const std::string& GetHistoryText() const noexcept;
		const std::string& GetRoomsText() const noexcept;
		std::uint32_t GetCurrentCount() const noexcept;
		std::uint32_t GetHistoryCount() const noexcept;
		std::uint32_t GetRoomCount() const noexcept;

	private:
		static std::string FormatMember(const DlModLobbyMemberV1& member);
		static std::string FormatMembers(const DlModLobbyMemberV1* members, std::uint32_t count);
		static std::string FormatRooms(const DlModLobbyRoomV1* rooms, std::uint32_t count);
		static const char* GetTeamText(const DlModLobbyMemberV1& member) noexcept;
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
		std::string currentText_ = "（暂无成员数据）";
		std::string historyText_ = "（暂无历史成员）";
		std::string roomsText_ = "（暂无房间搜索结果）";
		std::uint32_t currentCount_ = 0;
		std::uint32_t historyCount_ = 0;
		std::uint32_t roomCount_ = 0;
		std::string sessionFile_;
		std::string lastLobbyId_;
		std::vector<std::string> previousPuids_;
		bool sessionOpened_ = false;
	};
}
