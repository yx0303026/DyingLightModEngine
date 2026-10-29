#include "CheatModLobbyView.h"

#include <algorithm>
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

namespace cheat_mod
{
	namespace
	{
		// 页面公共样式：让标题、说明文字和分区边界形成稳定的信息层次。
		dl::imgui::Style MakeTitleStyle() noexcept
		{
			dl::imgui::Style style{};
			style.hasTextColor = true;
			style.textColor = ImVec4(0.88F, 0.94F, 0.98F, 1.00F);
			return style;
		}

		dl::imgui::Style MakeMutedStyle() noexcept
		{
			dl::imgui::Style style{};
			style.hasTextColor = true;
			style.textColor = ImVec4(0.58F, 0.67F, 0.74F, 1.00F);
			return style;
		}

		dl::imgui::Style MakeAccentStyle() noexcept
		{
			dl::imgui::Style style{};
			style.hasTextColor = true;
			style.textColor = ImVec4(0.35F, 0.82F, 0.98F, 1.00F);
			return style;
		}

		void ApplyStyle(dl::imgui::UIElement& element, const dl::imgui::Style& style)
		{
			element.SetStyle(style);
		}

		const char* SafeCellText(const char* value, const char* fallback) noexcept
		{
			return value != nullptr && value[0] != '\0' ? value : fallback;
		}

		std::vector<dl::imgui::DataTable::Row> MakeMemberRows(
			const std::vector<DlModLobbyMemberV1>& members,
			std::uint32_t count)
		{
			std::vector<dl::imgui::DataTable::Row> rows;
			const std::size_t rowCount = (std::min)(
				static_cast<std::size_t>(count), members.size());
			rows.reserve(rowCount);
			for (std::size_t index = 0; index < rowCount; ++index) {
				const DlModLobbyMemberV1& member = members[index];
				rows.push_back({
					SafeCellText(member.name, "未知"),
					SafeCellText(member.puid, "未报告"),
					CheatModLobby::GetTeamText(member),
					SafeCellText(member.steamId, "未报告"),
					std::to_string(member.hits)
				});
			}
			return rows;
		}

		std::vector<dl::imgui::DataTable::Row> MakeRoomRows(
			const std::vector<DlModLobbyRoomV1>& rooms,
			std::uint32_t count)
		{
			std::vector<dl::imgui::DataTable::Row> rows;
			const std::size_t rowCount = (std::min)(
				static_cast<std::size_t>(count), rooms.size());
			rows.reserve(rowCount);
			for (std::size_t index = 0; index < rowCount; ++index) {
				const DlModLobbyRoomV1& room = rooms[index];
				rows.push_back({
					SafeCellText(room.ownerName, "未知"),
					SafeCellText(room.lobbyId, "未报告"),
					SafeCellText(room.ownerSteamId, "未报告")
				});
			}
			return rows;
		}

		void ConfigureMemberTable(dl::imgui::DataTable& table)
		{
			table.SetColumns({
				{ "name", "名称", 150.0F },
				{ "puid", "PUID", 245.0F },
				{ "team", "阵营", 90.0F },
				{ "steam_id", "SteamID", 175.0F },
				{ "hits", "命中", 65.0F }
			});
			table.SetEmptyText("暂无成员");
		}

		void ConfigureRoomTable(dl::imgui::DataTable& table)
		{
			table.SetColumns({
				{ "owner", "房主", 155.0F },
				{ "lobby_id", "房号", 280.0F },
				{ "steam_id", "房主 SteamID", 185.0F }
			});
			table.SetEmptyText("暂无搜索结果");
		}
	}

	CheatModLobbyView::CheatModLobbyView(CheatModLobby& lobby) noexcept
		: lobby_(lobby)
	{
	}

	void CheatModLobbyView::BuildRoomListPage(dl::imgui::Page& page)
	{
		// 房间列表页：展示扫描状态、房间数量和可滚动的搜索结果。
		dl::imgui::Label& title = page.AddChild<dl::imgui::Label>("房间列表");
		ApplyStyle(title, MakeTitleStyle());
		dl::imgui::Label& subtitle = page.AddChild<dl::imgui::Label>("只读扫描 · 不建立连接 · 自动刷新");
		ApplyStyle(subtitle, MakeMutedStyle());
		page.AddChild<dl::imgui::Separator>();

		dl::imgui::Grid& summary = page.AddChild<dl::imgui::Grid>(1, 2);
		roomStatusLabel_ = &summary.AddAt<dl::imgui::Label>(0, 0, lobby_.GetStatusText());
		ApplyStyle(*roomStatusLabel_, MakeMutedStyle());
		roomCountLabel_ = &summary.AddAt<dl::imgui::Label>(0, 1, "房间\n0");
		ApplyStyle(*roomCountLabel_, MakeAccentStyle());
		page.AddChild<dl::imgui::Separator>();

		dl::imgui::Label& resultTitle = page.AddChild<dl::imgui::Label>("搜索结果");
		ApplyStyle(resultTitle, MakeTitleStyle());
		roomResultsTable_ = &page.AddChild<dl::imgui::DataTable>(ImVec2(0.0F, 0.0F), "暂无搜索结果");
		ConfigureRoomTable(*roomResultsTable_);
		roomResultsTable_->SetRows(MakeRoomRows(lobby_.GetRooms(), lobby_.GetRoomCount()));
	}

	void CheatModLobbyView::BuildMembersPage(dl::imgui::Page& page)
	{
		// 成员页面：把当前成员和历史成员分成独立分区，操作按钮集中在底部。
		dl::imgui::Label& title = page.AddChild<dl::imgui::Label>("房间成员");
		ApplyStyle(title, MakeTitleStyle());
		dl::imgui::Label& subtitle = page.AddChild<dl::imgui::Label>("当前房间成员与到场记录");
		ApplyStyle(subtitle, MakeMutedStyle());
		page.AddChild<dl::imgui::Separator>();

		dl::imgui::Grid& summary = page.AddChild<dl::imgui::Grid>(1, 2);
		currentCountLabel_ = &summary.AddAt<dl::imgui::Label>(0, 0, "当前成员\n0");
		ApplyStyle(*currentCountLabel_, MakeAccentStyle());
		historyCountLabel_ = &summary.AddAt<dl::imgui::Label>(0, 1, "历史成员\n0");
		ApplyStyle(*historyCountLabel_, MakeMutedStyle());
		page.AddChild<dl::imgui::Separator>();

		dl::imgui::Label& currentTitle = page.AddChild<dl::imgui::Label>("当前成员");
		ApplyStyle(currentTitle, MakeTitleStyle());
		currentMembersTable_ = &page.AddChild<dl::imgui::DataTable>(ImVec2(0.0F, 190.0F), "暂无当前成员");
		ConfigureMemberTable(*currentMembersTable_);
		currentMembersTable_->SetRows(MakeMemberRows(lobby_.GetCurrentMembers(), lobby_.GetCurrentCount()));

		dl::imgui::Label& historyTitle = page.AddChild<dl::imgui::Label>("历史成员");
		ApplyStyle(historyTitle, MakeTitleStyle());
		historyMembersTable_ = &page.AddChild<dl::imgui::DataTable>(ImVec2(0.0F, 105.0F), "暂无历史成员");
		ConfigureMemberTable(*historyMembersTable_);
		historyMembersTable_->SetRows(MakeMemberRows(lobby_.GetHistoryMembers(), lobby_.GetHistoryCount()));

		dl::imgui::StackPanel& actions = page.AddChild<dl::imgui::StackPanel>(dl::imgui::Orientation::Horizontal);
		dl::imgui::Button& clearHistory = actions.AddChild<dl::imgui::Button>("清空历史");
		clearHistory.SetOnClick(std::bind(&CheatModLobbyView::OnClearHistory, this));
		dl::imgui::Button& copyMembers = actions.AddChild<dl::imgui::Button>("复制当前成员");
		copyMembers.SetOnClick(std::bind(&CheatModLobbyView::OnCopyMembers, this));
	}

	void CheatModLobbyView::Refresh() noexcept
	{
		if (roomStatusLabel_ != nullptr)
			roomStatusLabel_->SetText(lobby_.GetStatusText());
		if (roomCountLabel_ != nullptr)
			roomCountLabel_->SetText(std::string("房间\n") + std::to_string(lobby_.GetRoomCount()));
		if (roomResultsTable_ != nullptr)
			roomResultsTable_->SetRows(MakeRoomRows(lobby_.GetRooms(), lobby_.GetRoomCount()));
		if (currentCountLabel_ != nullptr)
			currentCountLabel_->SetText(std::string("当前成员\n") + std::to_string(lobby_.GetCurrentCount()));
		if (currentMembersTable_ != nullptr)
			currentMembersTable_->SetRows(MakeMemberRows(lobby_.GetCurrentMembers(), lobby_.GetCurrentCount()));
		if (historyCountLabel_ != nullptr)
			historyCountLabel_->SetText(std::string("历史成员\n") + std::to_string(lobby_.GetHistoryCount()));
		if (historyMembersTable_ != nullptr)
			historyMembersTable_->SetRows(MakeMemberRows(lobby_.GetHistoryMembers(), lobby_.GetHistoryCount()));
	}

	void CheatModLobbyView::OnClearHistory() noexcept
	{
		lobby_.ClearHistory();
		Refresh();
	}

	void CheatModLobbyView::OnCopyMembers() noexcept
	{
		lobby_.CopyCurrentToClipboard();
	}
}
