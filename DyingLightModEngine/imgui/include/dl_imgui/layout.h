#pragma once

#include "dl_imgui/ui.h"

#include <functional>
#include <string>
#include <vector>

namespace dl::imgui {

	class StackPanel final : public Panel {
	public:
		explicit StackPanel(Orientation orientation = Orientation::Vertical);

		Orientation GetOrientation() const noexcept;
		void SetOrientation(Orientation orientation) noexcept;

	protected:
		void OnRender() override;

	private:
		Orientation orientation_ = Orientation::Vertical;
	};

	class Grid final : public Panel {
	public:
		struct Cell {
			UIElement* element = nullptr;
			int row = 0;
			int column = 0;
		};

		Grid(int rows = 1, int columns = 1);
		UIElement& AddAt(int row, int column, std::unique_ptr<UIElement> child);

		template <typename T, typename... Args>
		T& AddAt(int row, int column, Args&&... args)
		{
			auto child = std::make_unique<T>(std::forward<Args>(args)...);
			T& reference = *child;
			AddAt(row, column, std::move(child));
			return reference;
		}

		int GetRowCount() const noexcept;
		int GetColumnCount() const noexcept;
		void SetRowCount(int rows);
		void SetColumnCount(int columns);

	protected:
		void OnRender() override;

	private:
		int rows_ = 1;
		int columns_ = 1;
		std::vector<Cell> cells_;
		std::vector<std::unique_ptr<UIElement>> ownedChildren_;
	};

	class ScrollView final : public Panel {
	public:
		explicit ScrollView(ImVec2 size = ImVec2(0.0F, 0.0F), bool border = false);

		ImVec2 GetSize() const noexcept;
		void SetSize(ImVec2 size) noexcept;
		bool HasBorder() const noexcept;
		void SetBorder(bool border) noexcept;

	protected:
		void OnRender() override;

	private:
		ImVec2 size_{};
		bool border_ = false;
	};

	class ListView final : public UIElement {
	public:
		using SelectionChanged = std::function<void(int, const std::string&)>;

		void SetItems(std::vector<std::string> items);
		const std::vector<std::string>& GetItems() const noexcept;
		int GetSelectedIndex() const noexcept;
		void SetSelectedIndex(int index);
		void SetOnSelectionChanged(SelectionChanged callback);

	protected:
		void OnRender() override;

	private:
		std::vector<std::string> items_;
		int selectedIndex_ = -1;
		SelectionChanged selectionChanged_;
	};

} // namespace dl::imgui
