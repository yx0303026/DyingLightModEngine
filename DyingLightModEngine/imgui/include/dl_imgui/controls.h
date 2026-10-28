#pragma once

#include "dl_imgui/ui.h"

#include <functional>
#include <string>
#include <vector>

namespace dl::imgui {

	class Label final : public UIElement {
	public:
		explicit Label(std::string text = {});

		const std::string& GetText() const noexcept;
		void SetText(std::string text);

	protected:
		void OnRender() override;

	private:
		std::string text_;
	};

	class Button final : public UIElement {
	public:
		using Clicked = std::function<void()>;

		explicit Button(std::string text = {});

		const std::string& GetText() const noexcept;
		void SetText(std::string text);
		void SetOnClick(Clicked callback);

	protected:
		void OnRender() override;

	private:
		std::string text_;
		Clicked clicked_;
	};

	class CheckBox final : public UIElement {
	public:
		using Changed = std::function<void(bool)>;

		explicit CheckBox(std::string text = {}, bool checked = false);

		const std::string& GetText() const noexcept;
		void SetText(std::string text);
		bool IsChecked() const noexcept;
		void SetChecked(bool checked);
		void SetOnChanged(Changed callback);

	protected:
		void OnRender() override;

	private:
		std::string text_;
		bool checked_ = false;
		Changed changed_;
	};

	class ComboBox final : public UIElement {
	public:
		using SelectionChanged = std::function<void(int, const std::string&)>;

		explicit ComboBox(std::string label = {});

		const std::string& GetLabel() const noexcept;
		void SetLabel(std::string label);
		void SetItems(std::vector<std::string> items);
		const std::vector<std::string>& GetItems() const noexcept;
		int GetSelectedIndex() const noexcept;
		void SetSelectedIndex(int index);
		void SetOnSelectionChanged(SelectionChanged callback);

	protected:
		void OnRender() override;

	private:
		std::string label_;
		std::vector<std::string> items_;
		int selectedIndex_ = -1;
		SelectionChanged selectionChanged_;
	};

	class TextBox final : public UIElement {
	public:
		using Changed = std::function<void(const std::string&)>;

		explicit TextBox(std::string label = {}, bool multiline = false);

		const std::string& GetLabel() const noexcept;
		void SetLabel(std::string label);
		const std::string& GetText() const noexcept;
		void SetText(std::string text);
		bool IsMultiline() const noexcept;
		void SetMultiline(bool multiline) noexcept;
		void SetOnChanged(Changed callback);

	protected:
		void OnRender() override;

	private:
		std::string label_;
		std::string text_;
		bool multiline_ = false;
		std::vector<char> buffer_;
		Changed changed_;
	};

	class Slider final : public UIElement {
	public:
		using Changed = std::function<void(float)>;

		explicit Slider(std::string label = {}, float value = 0.0F, float minimum = 0.0F, float maximum = 1.0F);

		const std::string& GetLabel() const noexcept;
		void SetLabel(std::string label);
		float GetValue() const noexcept;
		void SetValue(float value);
		void SetRange(float minimum, float maximum);
		void SetOnChanged(Changed callback);

	protected:
		void OnRender() override;

	private:
		std::string label_;
		float value_ = 0.0F;
		float minimum_ = 0.0F;
		float maximum_ = 1.0F;
		Changed changed_;
	};

	class ProgressBar final : public UIElement {
	public:
		explicit ProgressBar(float value = 0.0F, std::string overlay = {});

		float GetValue() const noexcept;
		void SetValue(float value) noexcept;
		const std::string& GetOverlay() const noexcept;
		void SetOverlay(std::string overlay);

	protected:
		void OnRender() override;

	private:
		float value_ = 0.0F;
		std::string overlay_;
	};

	class Separator final : public UIElement {
	protected:
		void OnRender() override;
	};

} // namespace dl::imgui
