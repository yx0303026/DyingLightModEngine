#pragma once

#include "CheatModLobby.h"
#include "dl_imgui/imgui.h"

namespace cheat_mod
{
	// 大厅页面视图：只负责房间列表、成员列表的布局和数据刷新。
	class CheatModLobbyView final
	{
	public:
		explicit CheatModLobbyView(CheatModLobby& lobby) noexcept;
		~CheatModLobbyView() = default;
		CheatModLobbyView(const CheatModLobbyView&) = delete;
		CheatModLobbyView& operator=(const CheatModLobbyView&) = delete;

		void BuildRoomListPage(dl::imgui::Page& page);
		void BuildMembersPage(dl::imgui::Page& page);
		void Refresh() noexcept;

	private:
		void OnClearHistory() noexcept;
		void OnCopyMembers() noexcept;

		CheatModLobby& lobby_;
		dl::imgui::Label* roomStatusLabel_ = nullptr;
		dl::imgui::Label* roomCountLabel_ = nullptr;
		dl::imgui::ListView* roomResultsList_ = nullptr;
		dl::imgui::Label* currentCountLabel_ = nullptr;
		dl::imgui::ListView* currentMembersList_ = nullptr;
		dl::imgui::Label* historyCountLabel_ = nullptr;
		dl::imgui::ListView* historyMembersList_ = nullptr;
	};
}
