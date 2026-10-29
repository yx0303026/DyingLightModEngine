#pragma once

#include "dl_imgui/ui.h"

#include <functional>
#include <cstddef>
#include <string>
#include <vector>

namespace dl::imgui {

	// 数据表容器负责统一绘制表头、数据行、滚动区域和行选择。
	class DataTable final : public UIElement
	{
	public:
		struct Column
		{
			std::string id;
			std::string label;
			float width = 0.0F;
		};

		using Row = std::vector<std::string>;
		using SelectionChanged = std::function<void(int, const Row&)>;

		explicit DataTable(
			ImVec2 size = ImVec2(0.0F, 0.0F),
			std::string emptyText = "暂无数据");

		void SetColumns(std::vector<Column> columns);
		const std::vector<Column>& GetColumns() const noexcept;
		void SetRows(std::vector<Row> rows);
		const std::vector<Row>& GetRows() const noexcept;

		ImVec2 GetSize() const noexcept;
		void SetSize(ImVec2 size) noexcept;
		const std::string& GetEmptyText() const noexcept;
		void SetEmptyText(std::string text);

		bool IsHeaderVisible() const noexcept;
		void SetHeaderVisible(bool visible) noexcept;
		bool IsStriped() const noexcept;
		void SetStriped(bool striped) noexcept;
		bool HasBorders() const noexcept;
		void SetBorders(bool borders) noexcept;

		int GetSelectedIndex() const noexcept;
		void SetSelectedIndex(int index);
		void SetOnSelectionChanged(SelectionChanged callback);

	protected:
		void OnRender() override;

	private:
		static const char* GetCellText(const Row& row, std::size_t column) noexcept;
		void RenderEmptyState() const;
		void RenderHeader();
		void RenderRows();

		ImVec2 size_{};
		std::vector<Column> columns_;
		std::vector<Row> rows_;
		std::string emptyText_;
		SelectionChanged selectionChanged_;
		int selectedIndex_ = -1;
		bool headerVisible_ = true;
		bool striped_ = true;
		bool borders_ = true;
	};

} // dl::imgui 命名空间
