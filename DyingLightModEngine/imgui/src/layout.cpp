#include "dl_imgui/layout.h"

#include <algorithm>
#include <stdexcept>

namespace dl::imgui {

	StackPanel::StackPanel(Orientation orientation)
		: orientation_(orientation)
	{
	}

	Orientation StackPanel::GetOrientation() const noexcept
	{
		return orientation_;
	}

	void StackPanel::SetOrientation(Orientation orientation) noexcept
	{
		orientation_ = orientation;
	}

	void StackPanel::OnRender()
	{
		bool hasRenderedChild = false;
		for (const auto& child : Children()) {
			if (child->GetVisibility() == Visibility::Collapsed) {
				continue;
			}
			if (orientation_ == Orientation::Horizontal && hasRenderedChild) {
				ImGui::SameLine();
			}
			child->Render();
			hasRenderedChild = true;
		}
	}

	Grid::Grid(int rows, int columns)
	{
		SetRowCount(rows);
		SetColumnCount(columns);
	}

	UIElement& Grid::AddAt(int row, int column, std::unique_ptr<UIElement> child)
	{
		if (row < 0 || column < 0 || row >= rows_ || column >= columns_ || child == nullptr) {
			throw std::invalid_argument("Grid cell or child is invalid");
		}
		UIElement& reference = *child;
		UIElement* element = child.get();
		ownedChildren_.push_back(std::move(child));
		cells_.push_back({ element, row, column });
		return reference;
	}

	int Grid::GetRowCount() const noexcept
	{
		return rows_;
	}

	int Grid::GetColumnCount() const noexcept
	{
		return columns_;
	}

	void Grid::SetRowCount(int rows)
	{
		if (rows < 1) {
			throw std::invalid_argument("Grid must have at least one row");
		}
		for (const Cell& cell : cells_) {
			if (cell.row >= rows) {
				throw std::invalid_argument("Grid row count cannot exclude an occupied cell");
			}
		}
		rows_ = rows;
	}

	void Grid::SetColumnCount(int columns)
	{
		if (columns < 1) {
			throw std::invalid_argument("Grid must have at least one column");
		}
		for (const Cell& cell : cells_) {
			if (cell.column >= columns) {
				throw std::invalid_argument("Grid column count cannot exclude an occupied cell");
			}
		}
		columns_ = columns;
	}

	void Grid::OnRender()
	{
		if (!ImGui::BeginTable("##grid", columns_, ImGuiTableFlags_SizingStretchSame)) {
			return;
		}

		for (int column = 0; column < columns_; ++column) {
			ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);
		}

		for (int row = 0; row < rows_; ++row) {
			ImGui::TableNextRow();
			for (int column = 0; column < columns_; ++column) {
				ImGui::TableSetColumnIndex(column);
				for (const Cell& cell : cells_) {
					if (cell.row == row && cell.column == column) {
						cell.element->Render();
					}
				}
			}
		}

		ImGui::EndTable();
	}

	ScrollView::ScrollView(ImVec2 size, bool border)
		: size_(size)
		, border_(border)
	{
	}

	ImVec2 ScrollView::GetSize() const noexcept
	{
		return size_;
	}

	void ScrollView::SetSize(ImVec2 size) noexcept
	{
		size_ = size;
	}

	bool ScrollView::HasBorder() const noexcept
	{
		return border_;
	}

	void ScrollView::SetBorder(bool border) noexcept
	{
		border_ = border;
	}

	void ScrollView::OnRender()
	{
		const ImVec2 size = ResolveSize(size_);
		if (!ImGui::BeginChild("##scroll_view", size, border_)) {
			ImGui::EndChild();
			return;
		}
		Panel::OnRender();
		ImGui::EndChild();
	}

	ListView::ListView(ImVec2 size, std::string emptyText)
		: size_(size)
		, emptyText_(std::move(emptyText))
	{
	}

	void ListView::SetItems(std::vector<std::string> items)
	{
		items_ = std::move(items);
		if (selectedIndex_ >= static_cast<int>(items_.size())) {
			SetSelectedIndex(items_.empty() ? -1 : static_cast<int>(items_.size()) - 1);
		}
	}

	const std::vector<std::string>& ListView::GetItems() const noexcept
	{
		return items_;
	}

	int ListView::GetSelectedIndex() const noexcept
	{
		return selectedIndex_;
	}

	void ListView::SetSelectedIndex(int index)
	{
		const int normalized = index >= 0 && index < static_cast<int>(items_.size()) ? index : -1;
		if (normalized == selectedIndex_) {
			return;
		}
		selectedIndex_ = normalized;
		if (selectionChanged_) {
			selectionChanged_(selectedIndex_, selectedIndex_ >= 0 ? items_[selectedIndex_] : std::string{});
		}
	}

	void ListView::SetOnSelectionChanged(SelectionChanged callback)
	{
		selectionChanged_ = std::move(callback);
	}

	ImVec2 ListView::GetSize() const noexcept
	{
		return size_;
	}

	void ListView::SetSize(ImVec2 size) noexcept
	{
		size_ = size;
	}

	const std::string& ListView::GetEmptyText() const noexcept
	{
		return emptyText_;
	}

	void ListView::SetEmptyText(std::string text)
	{
		emptyText_ = std::move(text);
	}

	void ListView::OnRender()
	{
		const ImVec2 size = ResolveSize(size_);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ScaleSize(8.0F), ScaleSize(6.0F)));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ScaleSize(9.0F), ScaleSize(7.0F)));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0F, ScaleSize(3.0F)));
		if (!ImGui::BeginChild("##list_view", size, true)) {
			ImGui::EndChild();
			ImGui::PopStyleVar(3);
			return;
		}

		if (items_.empty()) {
			ImGui::Dummy(ImVec2(0.0F, ScaleSize(5.0F)));
			ImGui::TextDisabled("%s", emptyText_.c_str());
		} else {
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(items_.size()));
			while (clipper.Step()) {
				for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
					ImGui::PushID(index);
					const bool selected = selectedIndex_ == index;
					const ImVec2 rowSize(std::max(0.0F, ImGui::GetContentRegionAvail().x), 0.0F);
					if (ImGui::Selectable(items_[index].c_str(), selected, ImGuiSelectableFlags_None, rowSize)) {
						SetSelectedIndex(index);
					}
					ImGui::PopID();
				}
			}
		}

		ImGui::EndChild();
		ImGui::PopStyleVar(3);
	}

} // namespace dl::imgui
