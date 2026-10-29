#include "dl_imgui/navigation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dl::imgui {
	namespace {

		constexpr float kPi = 3.14159265359F;

		void DrawNavigationIcon(
			ImDrawList& drawList,
			const ImVec2& center,
			float size,
			NavigationIcon icon,
			ImU32 color)
		{
			const float half = size * 0.5F;
			const float stroke = std::max(1.0F, size * 0.095F);
			switch (icon) {
			case NavigationIcon::Home:
				drawList.AddTriangleFilled(
					ImVec2(center.x, center.y - half * 0.78F),
					ImVec2(center.x - half * 0.82F, center.y - half * 0.05F),
					ImVec2(center.x + half * 0.82F, center.y - half * 0.05F),
					color);
				drawList.AddRectFilled(
					ImVec2(center.x - half * 0.58F, center.y - half * 0.02F),
					ImVec2(center.x + half * 0.58F, center.y + half * 0.72F),
					color,
					size * 0.08F);
				drawList.AddRectFilled(
					ImVec2(center.x - half * 0.13F, center.y + half * 0.27F),
					ImVec2(center.x + half * 0.13F, center.y + half * 0.72F),
					ImGui::GetColorU32(ImGuiCol_ChildBg),
					size * 0.025F);
				break;
			case NavigationIcon::Person:
				drawList.AddCircleFilled(
					ImVec2(center.x, center.y - half * 0.36F),
					size * 0.16F,
					color);
				drawList.PathArcTo(
					ImVec2(center.x, center.y + half * 0.12F),
					size * 0.33F,
					kPi,
					kPi * 2.0F,
					16);
				drawList.PathStroke(color, false, stroke);
				break;
			case NavigationIcon::Settings:
				drawList.AddCircle(center, size * 0.28F, color, 10, stroke);
				drawList.AddCircleFilled(center, size * 0.10F, color);
				for (int spoke = 0; spoke < 8; ++spoke) {
					const float angle = static_cast<float>(spoke) * kPi / 4.0F;
					const ImVec2 inner(
						center.x + std::cos(angle) * size * 0.28F,
						center.y + std::sin(angle) * size * 0.28F);
					const ImVec2 outer(
						center.x + std::cos(angle) * size * 0.43F,
						center.y + std::sin(angle) * size * 0.43F);
					drawList.AddLine(inner, outer, color, stroke);
				}
				break;
			case NavigationIcon::Game:
				drawList.AddRect(
					ImVec2(center.x - half * 0.78F, center.y - half * 0.42F),
					ImVec2(center.x + half * 0.78F, center.y + half * 0.42F),
					color,
					size * 0.18F,
					0,
					stroke);
				drawList.AddLine(
					ImVec2(center.x - half * 0.42F, center.y),
					ImVec2(center.x - half * 0.10F, center.y),
					color,
					stroke);
				drawList.AddLine(
					ImVec2(center.x - half * 0.26F, center.y - half * 0.16F),
					ImVec2(center.x - half * 0.26F, center.y + half * 0.16F),
					color,
					stroke);
				drawList.AddCircleFilled(
					ImVec2(center.x + half * 0.35F, center.y - half * 0.12F),
					size * 0.055F,
					color);
				drawList.AddCircleFilled(
					ImVec2(center.x + half * 0.52F, center.y + half * 0.12F),
					size * 0.055F,
					color);
				break;
			case NavigationIcon::Tools:
				drawList.AddLine(
					ImVec2(center.x - half * 0.50F, center.y + half * 0.50F),
					ImVec2(center.x + half * 0.42F, center.y - half * 0.42F),
					color,
					stroke);
				drawList.AddCircle(center, size * 0.24F, color, 8, stroke);
				drawList.AddCircleFilled(
					ImVec2(center.x - half * 0.50F, center.y + half * 0.50F),
					size * 0.15F,
					color);
				break;
			case NavigationIcon::None:
			default:
				break;
			}
		}

	} // 匿名命名空间

	Page::Page(std::string title)
		: title_(std::move(title))
	{
	}

	const std::string& Page::GetTitle() const noexcept
	{
		return title_;
	}

	void Page::SetTitle(std::string title)
	{
		title_ = std::move(title);
	}

	void Page::OnRender()
	{
		Panel::OnRender();
	}

	void NavigationBar::AddItem(std::string id, std::string label)
	{
		AddItem(std::move(id), std::move(label), NavigationIcon::None);
	}

	void NavigationBar::AddItem(std::string id, std::string label, NavigationIcon icon)
	{
		if (id.empty()) {
			throw std::invalid_argument("Navigation item id cannot be empty");
		}
		for (const Item& item : items_) {
			if (item.id == id) {
				throw std::invalid_argument("Navigation item id must be unique");
			}
		}
		if (selectedId_.empty()) {
			selectedId_ = id;
		}
		items_.push_back({ std::move(id), std::move(label), icon });
	}

	const std::vector<NavigationBar::Item>& NavigationBar::GetItems() const noexcept
	{
		return items_;
	}

	const std::string& NavigationBar::GetSelectedId() const noexcept
	{
		return selectedId_;
	}

	void NavigationBar::SetSelectedId(std::string id)
	{
		bool found = false;
		for (const Item& item : items_) {
			if (item.id == id) {
				found = true;
				break;
			}
		}
		if (!found || selectedId_ == id) {
			return;
		}
		selectedId_ = std::move(id);
		if (selectionChanged_) {
			selectionChanged_(selectedId_);
		}
	}

	void NavigationBar::SetItemIcon(const std::string& id, NavigationIcon icon) noexcept
	{
		for (Item& item : items_) {
			if (item.id == id) {
				item.icon = icon;
				return;
			}
		}
	}

	void NavigationBar::SetOnSelectionChanged(SelectionChanged callback)
	{
		selectionChanged_ = std::move(callback);
	}

	void NavigationBar::OnRender()
	{
		const float rowHeight = ScaleSize(42.0F);
		const float iconSize = ScaleSize(21.0F);
		const float iconOffset = ScaleSize(28.0F);
		const float labelOffset = ScaleSize(52.0F);
		const float accentWidth = ScaleSize(3.0F);
		const float itemSpacing = ScaleSize(5.0F);
		const float rounding = ScaleSize(6.0F);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0F, itemSpacing));
		for (const Item& item : items_) {
			ImGui::PushID(item.id.c_str());
			const float itemWidth = std::max(0.0F, ImGui::GetContentRegionAvail().x);
			const ImVec2 itemPosition = ImGui::GetCursorScreenPos();
			const bool selected = selectedId_ == item.id;
			const bool pressed = ImGui::InvisibleButton("##navigation_item", ImVec2(itemWidth, rowHeight));
			const bool hovered = ImGui::IsItemHovered();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			if (selected || hovered) {
				const ImU32 background = ImGui::GetColorU32(
					selected ? ImGuiCol_Header : ImGuiCol_HeaderHovered);
				drawList->AddRectFilled(
					itemPosition,
					ImVec2(itemPosition.x + itemWidth, itemPosition.y + rowHeight),
					background,
					rounding);
			}
			if (selected) {
				drawList->AddRectFilled(
					ImVec2(itemPosition.x, itemPosition.y + ScaleSize(5.0F)),
					ImVec2(itemPosition.x + accentWidth, itemPosition.y + rowHeight - ScaleSize(5.0F)),
					ImGui::GetColorU32(ImGuiCol_NavHighlight),
					accentWidth * 0.5F);
			}
			const ImU32 iconColor = ImGui::GetColorU32(
				selected ? ImGuiCol_NavHighlight : ImGuiCol_TextDisabled);
			DrawNavigationIcon(
				*drawList,
				ImVec2(itemPosition.x + iconOffset, itemPosition.y + rowHeight * 0.5F),
				iconSize,
				item.icon,
				iconColor);
			ImVec4 inactiveText = ImGui::GetStyle().Colors[ImGuiCol_Text];
			inactiveText.w *= 0.78F;
			const ImU32 textColor = ImGui::GetColorU32(
				selected ? ImGui::GetStyle().Colors[ImGuiCol_Text] : inactiveText);
			drawList->AddText(
				ImVec2(itemPosition.x + labelOffset, itemPosition.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5F),
				textColor,
				item.label.c_str());
			if (pressed) {
				SetSelectedId(item.id);
			}
			ImGui::PopID();
		}
		ImGui::PopStyleVar();
	}

	NavigationView::NavigationView(float navigationWidth)
		: navigationWidth_(navigationWidth)
	{
	}

	NavigationBar& NavigationView::GetNavigationBar() noexcept
	{
		return navigationBar_;
	}

	const NavigationBar& NavigationView::GetNavigationBar() const noexcept
	{
		return navigationBar_;
	}

	Page& NavigationView::AddPage(std::string id, std::unique_ptr<Page> page)
	{
		if (id.empty() || page == nullptr || FindPage(id) != nullptr) {
			throw std::invalid_argument("Navigation page id must be unique and page cannot be null");
		}
		Page& reference = *page;
		navigationBar_.AddItem(id, page->GetTitle().empty() ? id : page->GetTitle());
		pages_.emplace_back(std::move(id), std::move(page));
		return reference;
	}

	Page* NavigationView::FindPage(const std::string& id) noexcept
	{
		for (auto& entry : pages_) {
			if (entry.first == id) {
				return entry.second.get();
			}
		}
		return nullptr;
	}

	const Page* NavigationView::FindPage(const std::string& id) const noexcept
	{
		for (const auto& entry : pages_) {
			if (entry.first == id) {
				return entry.second.get();
			}
		}
		return nullptr;
	}

	float NavigationView::GetNavigationWidth() const noexcept
	{
		return navigationWidth_;
	}

	void NavigationView::SetNavigationWidth(float width) noexcept
	{
		navigationWidth_ = width < 0.0F ? 0.0F : width;
	}

	void NavigationView::OnRender()
	{
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float width = navigationWidth_ > 0.0F ? ScaleSize(navigationWidth_) : 1.0F;
		const float columnSpacing = ScaleSize(14.0F);
		const float contentWidth = std::max(0.0F, available.x - width - columnSpacing);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ScaleSize(12.0F), ScaleSize(14.0F)));
		ImGui::BeginChild("##navigation_bar", ImVec2(width, available.y), false);
		navigationBar_.Render();
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::SameLine(0.0F, columnSpacing);
		ImGui::BeginChild("##navigation_page", ImVec2(contentWidth, available.y), false);
		if (Page* page = FindPage(navigationBar_.GetSelectedId())) {
			page->Render();
		}
		ImGui::EndChild();
	}

	Page& TabView::AddTab(std::string label, std::unique_ptr<Page> page)
	{
		if (page == nullptr) {
			throw std::invalid_argument("Tab page cannot be null");
		}
		Page& reference = *page;
		tabs_.push_back({ std::move(label), std::move(page) });
		return reference;
	}

	int TabView::GetSelectedIndex() const noexcept
	{
		return selectedIndex_;
	}

	void TabView::SetSelectedIndex(int index)
	{
		if (index >= 0 && index < static_cast<int>(tabs_.size())) {
			selectedIndex_ = index;
		}
	}

	void TabView::OnRender()
	{
		if (!ImGui::BeginTabBar("##tab_view")) {
			return;
		}

		for (int index = 0; index < static_cast<int>(tabs_.size()); ++index) {
			Tab& tab = tabs_[index];
			const ImGuiTabItemFlags flags = index == selectedIndex_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
			if (ImGui::BeginTabItem(tab.label.c_str(), nullptr, flags)) {
				selectedIndex_ = index;
				tab.page->Render();
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

} // dl::imgui 命名空间
