#include "dl_imgui/navigation.h"

#include <algorithm>
#include <stdexcept>

namespace dl::imgui {

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
		items_.push_back({ std::move(id), std::move(label) });
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

	void NavigationBar::SetOnSelectionChanged(SelectionChanged callback)
	{
		selectionChanged_ = std::move(callback);
	}

	void NavigationBar::OnRender()
	{
		bool first = true;
		for (const Item& item : items_) {
			if (!first) {
				ImGui::SameLine();
			}
			first = false;
			if (ImGui::Selectable(item.label.c_str(), selectedId_ == item.id)) {
				SetSelectedId(item.id);
			}
		}
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
		const float width = navigationWidth_ > 0.0F ? navigationWidth_ : 1.0F;
		const float contentWidth = std::max(0.0F, available.x - width - ImGui::GetStyle().ItemSpacing.x);

		ImGui::BeginChild("##navigation_bar", ImVec2(width, available.y), false);
		navigationBar_.Render();
		ImGui::EndChild();
		ImGui::SameLine();
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

} // namespace dl::imgui
