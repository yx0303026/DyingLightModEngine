#pragma once

#include "dl_imgui/layout.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dl::imgui {

	enum class NavigationIcon {
		None,
		Home,
		Person,
		Settings,
		Game,
		Tools
	};

	class Page : public Panel {
	public:
		explicit Page(
			std::string title,
			std::string subtitle,
			ImVec2 padding);

		const std::string& GetTitle() const noexcept;
		void SetTitle(std::string title);
		const std::string& GetSubtitle() const noexcept;
		void SetSubtitle(std::string subtitle);
		ImVec2 GetPadding() const noexcept;
		void SetPadding(ImVec2 padding) noexcept;

	protected:
		void OnRender() override;

	private:
		std::string title_;
		std::string subtitle_;
		ImVec2 padding_ = ImVec2(16.0F, 16.0F);
	};

	class NavigationBar final : public UIElement {
	public:
		struct Item {
			std::string id;
			std::string label;
			NavigationIcon icon = NavigationIcon::None;
		};

		using SelectionChanged = std::function<void(const std::string&)>;

		void AddItem(std::string id, std::string label);
		void AddItem(std::string id, std::string label, NavigationIcon icon);
		const std::vector<Item>& GetItems() const noexcept;
		const std::string& GetSelectedId() const noexcept;
		void SetSelectedId(std::string id);
		void SetItemIcon(const std::string& id, NavigationIcon icon) noexcept;
		void SetOnSelectionChanged(SelectionChanged callback);

	protected:
		void OnRender() override;

	private:
		std::vector<Item> items_;
		std::string selectedId_;
		SelectionChanged selectionChanged_;
	};

	class NavigationView final : public UIElement {
	public:
		explicit NavigationView(float navigationWidth = 180.0F);

		NavigationBar& GetNavigationBar() noexcept;
		const NavigationBar& GetNavigationBar() const noexcept;
		Page& AddPage(std::string id, std::unique_ptr<Page> page);

		template <typename... Args>
		Page& AddPage(std::string id, Args&&... args)
		{
			auto page = std::make_unique<Page>(std::forward<Args>(args)...);
			return AddPage(std::move(id), std::move(page));
		}

		Page* FindPage(const std::string& id) noexcept;
		const Page* FindPage(const std::string& id) const noexcept;
		float GetNavigationWidth() const noexcept;
		void SetNavigationWidth(float width) noexcept;

	protected:
		void OnRender() override;

	private:
		NavigationBar navigationBar_;
		std::vector<std::pair<std::string, std::unique_ptr<Page>>> pages_;
		float navigationWidth_ = 180.0F;
	};

	class TabView final : public UIElement {
	public:
		struct Tab {
			std::string label;
			std::unique_ptr<Page> page;
		};

		Page& AddTab(std::string label, std::unique_ptr<Page> page);

		template <typename... Args>
		Page& AddTab(std::string label, Args&&... args)
		{
			auto page = std::make_unique<Page>(std::forward<Args>(args)...);
			return AddTab(std::move(label), std::move(page));
		}

		int GetSelectedIndex() const noexcept;
		void SetSelectedIndex(int index);

	protected:
		void OnRender() override;

	private:
		std::vector<Tab> tabs_;
		int selectedIndex_ = 0;
	};

} // dl::imgui 命名空间
