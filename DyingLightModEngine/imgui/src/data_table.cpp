#include "dl_imgui/data_table.h"

#include <algorithm>
#include <utility>

namespace dl::imgui
{

	DataTable::DataTable(ImVec2 size, std::string emptyText)
		: size_(size)
		, emptyText_(std::move(emptyText))
	{
	}

	void DataTable::SetColumns(std::vector<Column> columns)
	{
		columns_ = std::move(columns);
	}

	const std::vector<DataTable::Column>& DataTable::GetColumns() const noexcept
	{
		return columns_;
	}

	void DataTable::SetRows(std::vector<Row> rows)
	{
		rows_ = std::move(rows);
		if (selectedIndex_ >= static_cast<int>(rows_.size())) {
			selectedIndex_ = -1;
		}
	}

	const std::vector<DataTable::Row>& DataTable::GetRows() const noexcept
	{
		return rows_;
	}

	ImVec2 DataTable::GetSize() const noexcept
	{
		return size_;
	}

	void DataTable::SetSize(ImVec2 size) noexcept
	{
		size_.x = std::max(0.0F, size.x);
		size_.y = std::max(0.0F, size.y);
	}

	const std::string& DataTable::GetEmptyText() const noexcept
	{
		return emptyText_;
	}

	void DataTable::SetEmptyText(std::string text)
	{
		emptyText_ = std::move(text);
	}

	bool DataTable::IsHeaderVisible() const noexcept
	{
		return headerVisible_;
	}

	void DataTable::SetHeaderVisible(bool visible) noexcept
	{
		headerVisible_ = visible;
	}

	bool DataTable::IsStriped() const noexcept
	{
		return striped_;
	}

	void DataTable::SetStriped(bool striped) noexcept
	{
		striped_ = striped;
	}

	bool DataTable::HasBorders() const noexcept
	{
		return borders_;
	}

	void DataTable::SetBorders(bool borders) noexcept
	{
		borders_ = borders;
	}

	int DataTable::GetSelectedIndex() const noexcept
	{
		return selectedIndex_;
	}

	void DataTable::SetSelectedIndex(int index)
	{
		const int normalized = index >= 0 && index < static_cast<int>(rows_.size()) ? index : -1;
		if (normalized == selectedIndex_) {
			return;
		}
		selectedIndex_ = normalized;
		if (selectionChanged_) {
			selectionChanged_(selectedIndex_, selectedIndex_ >= 0 ? rows_[selectedIndex_] : Row{});
		}
	}

	void DataTable::SetOnSelectionChanged(SelectionChanged callback)
	{
		selectionChanged_ = std::move(callback);
	}

	const char* DataTable::GetCellText(const Row& row, std::size_t column) noexcept
	{
		return column < row.size() ? row[column].c_str() : "";
	}

	void DataTable::RenderEmptyState() const
	{
		ImGui::TextDisabled("%s", emptyText_.c_str());
	}

	void DataTable::RenderHeader()
	{
		if (headerVisible_) {
			ImGui::TableHeadersRow();
		}
	}

	void DataTable::RenderRows()
	{
		const float rowHeight = ImGui::GetTextLineHeight() + ScaleSize(12.0F);
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(rows_.size()), rowHeight);
		while (clipper.Step()) {
			for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
				const Row& row = rows_[static_cast<std::size_t>(index)];
				const bool selected = selectedIndex_ == index;
				ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
				for (std::size_t column = 0; column < columns_.size(); ++column) {
					ImGui::TableSetColumnIndex(static_cast<int>(column));
					const char* text = GetCellText(row, column);
					if (column == 0U) {
						ImGui::PushID(index);
						const bool pressed = ImGui::Selectable(
							text,
							selected,
							ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
							ImVec2(0.0F, rowHeight));
						ImGui::PopID();
						if (pressed) {
							SetSelectedIndex(index);
						}
					} else {
						ImGui::TextUnformatted(text);
					}
				}
			}
		}
	}

	void DataTable::OnRender()
	{
		if (columns_.empty()) {
			RenderEmptyState();
			return;
		}

		const ImVec2 available = ImGui::GetContentRegionAvail();
		ImVec2 tableSize = ResolveSize(size_);
		if (size_.y <= 0.0F && GetLayout().height < 0.0F) {
			tableSize.y = std::max(0.0F, available.y);
		}

		ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollX;
		if (tableSize.y > 0.0F) {
			flags |= ImGuiTableFlags_ScrollY;
		}
		if (striped_) {
			flags |= ImGuiTableFlags_RowBg;
		}
		if (borders_) {
			flags |= ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV;
		}

		ImGui::PushStyleVar(
			ImGuiStyleVar_CellPadding,
			ImVec2(ScaleSize(10.0F), ScaleSize(6.0F)));
		if (!ImGui::BeginTable("##data_table", static_cast<int>(columns_.size()), flags, tableSize)) {
			ImGui::PopStyleVar();
			return;
		}

		for (const Column& column : columns_) {
			const ImGuiTableColumnFlags columnFlags =
				column.width > 0.0F ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch;
			const std::string columnLabel = column.id.empty()
				? column.label
				: column.label + "##" + column.id;
			ImGui::TableSetupColumn(
				columnLabel.c_str(),
				columnFlags,
				column.width > 0.0F ? ScaleSize(column.width) : 0.0F);
		}
		if (headerVisible_ && tableSize.y > 0.0F) {
			ImGui::TableSetupScrollFreeze(0, 1);
		}
		RenderHeader();
		if (rows_.empty()) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			RenderEmptyState();
		} else {
			RenderRows();
		}
		ImGui::EndTable();
		ImGui::PopStyleVar();
	}

} // dl::imgui 命名空间
