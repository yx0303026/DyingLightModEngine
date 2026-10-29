#include "CheatModLobbyView.h"

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

		std::vector<std::string> SplitLines(const std::string& text)
		{
			std::vector<std::string> lines;
			std::size_t begin = 0;
			while (begin <= text.size()) {
				const std::size_t end = text.find('\n', begin);
				const std::size_t length = end == std::string::npos ? text.size() - begin : end - begin;
				if (length > 0U) {
					lines.emplace_back(text, begin, length);
				}
				if (end == std::string::npos) {
					break;
				}
				begin = end + 1U;
			}
			return lines;
		}

		std::vector<std::string> MakeListItems(const std::string& text, std::uint32_t count)
		{
			return count == 0U ? std::vector<std::string>{} : SplitLines(text);
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
		roomResultsList_ = &page.AddChild<dl::imgui::ListView>(ImVec2(0.0F, 0.0F), "暂无搜索结果");
		roomResultsList_->SetItems(MakeListItems(lobby_.GetRoomsText(), lobby_.GetRoomCount()));
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
		currentMembersList_ = &page.AddChild<dl::imgui::ListView>(ImVec2(0.0F, 190.0F), "暂无当前成员");
		currentMembersList_->SetItems(MakeListItems(lobby_.GetCurrentText(), lobby_.GetCurrentCount()));

		dl::imgui::Label& historyTitle = page.AddChild<dl::imgui::Label>("历史成员");
		ApplyStyle(historyTitle, MakeTitleStyle());
		historyMembersList_ = &page.AddChild<dl::imgui::ListView>(ImVec2(0.0F, 105.0F), "暂无历史成员");
		historyMembersList_->SetItems(MakeListItems(lobby_.GetHistoryText(), lobby_.GetHistoryCount()));

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
		if (roomResultsList_ != nullptr)
			roomResultsList_->SetItems(MakeListItems(lobby_.GetRoomsText(), lobby_.GetRoomCount()));
		if (currentCountLabel_ != nullptr)
			currentCountLabel_->SetText(std::string("当前成员\n") + std::to_string(lobby_.GetCurrentCount()));
		if (currentMembersList_ != nullptr)
			currentMembersList_->SetItems(MakeListItems(lobby_.GetCurrentText(), lobby_.GetCurrentCount()));
		if (historyCountLabel_ != nullptr)
			historyCountLabel_->SetText(std::string("历史成员\n") + std::to_string(lobby_.GetHistoryCount()));
		if (historyMembersList_ != nullptr)
			historyMembersList_->SetItems(MakeListItems(lobby_.GetHistoryText(), lobby_.GetHistoryCount()));
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
