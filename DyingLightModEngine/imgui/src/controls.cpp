#include "dl_imgui/controls.h"

#include <algorithm>

namespace dl::imgui {
	namespace {

		ImVec2 ResolveControlSize(const LayoutOptions& layout)
		{
			return ImVec2(
				layout.width >= 0.0F ? layout.width : 0.0F,
				layout.height >= 0.0F ? layout.height : 0.0F);
		}

	} // namespace

	Label::Label(std::string text)
		: text_(std::move(text))
	{
	}

	const std::string& Label::GetText() const noexcept
	{
		return text_;
	}

	void Label::SetText(std::string text)
	{
		text_ = std::move(text);
	}

	void Label::OnRender()
	{
		ImGui::TextUnformatted(text_.c_str());
	}

	Button::Button(std::string text)
		: text_(std::move(text))
	{
	}

	const std::string& Button::GetText() const noexcept
	{
		return text_;
	}

	void Button::SetText(std::string text)
	{
		text_ = std::move(text);
	}

	void Button::SetOnClick(Clicked callback)
	{
		clicked_ = std::move(callback);
	}

	void Button::OnRender()
	{
		const ImVec2 size = ResolveControlSize(GetLayout());
		if (ImGui::Button(text_.c_str(), size) && clicked_) {
			clicked_();
		}
	}

	CheckBox::CheckBox(std::string text, bool checked)
		: text_(std::move(text))
		, checked_(checked)
	{
	}

	const std::string& CheckBox::GetText() const noexcept
	{
		return text_;
	}

	void CheckBox::SetText(std::string text)
	{
		text_ = std::move(text);
	}

	bool CheckBox::IsChecked() const noexcept
	{
		return checked_;
	}

	void CheckBox::SetChecked(bool checked)
	{
		if (checked_ == checked) {
			return;
		}
		checked_ = checked;
		if (changed_) {
			changed_(checked_);
		}
	}

	void CheckBox::SetOnChanged(Changed callback)
	{
		changed_ = std::move(callback);
	}

	void CheckBox::OnRender()
	{
		if (ImGui::Checkbox(text_.c_str(), &checked_) && changed_) {
			changed_(checked_);
		}
	}

	ComboBox::ComboBox(std::string label)
		: label_(std::move(label))
	{
	}

	const std::string& ComboBox::GetLabel() const noexcept
	{
		return label_;
	}

	void ComboBox::SetLabel(std::string label)
	{
		label_ = std::move(label);
	}

	void ComboBox::SetItems(std::vector<std::string> items)
	{
		items_ = std::move(items);
		if (selectedIndex_ >= static_cast<int>(items_.size())) {
			SetSelectedIndex(items_.empty() ? -1 : static_cast<int>(items_.size()) - 1);
		}
	}

	const std::vector<std::string>& ComboBox::GetItems() const noexcept
	{
		return items_;
	}

	int ComboBox::GetSelectedIndex() const noexcept
	{
		return selectedIndex_;
	}

	void ComboBox::SetSelectedIndex(int index)
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

	void ComboBox::SetOnSelectionChanged(SelectionChanged callback)
	{
		selectionChanged_ = std::move(callback);
	}

	void ComboBox::OnRender()
	{
		if (GetLayout().width >= 0.0F) {
			ImGui::SetNextItemWidth(GetLayout().width);
		}
		const char* preview = selectedIndex_ >= 0 ? items_[selectedIndex_].c_str() : "";
		if (!ImGui::BeginCombo(label_.c_str(), preview)) {
			return;
		}

		for (int index = 0; index < static_cast<int>(items_.size()); ++index) {
			const bool selected = selectedIndex_ == index;
			if (ImGui::Selectable(items_[index].c_str(), selected)) {
				SetSelectedIndex(index);
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	TextBox::TextBox(std::string label, bool multiline)
		: label_(std::move(label))
		, multiline_(multiline)
	{
	}

	const std::string& TextBox::GetLabel() const noexcept
	{
		return label_;
	}

	void TextBox::SetLabel(std::string label)
	{
		label_ = std::move(label);
	}

	const std::string& TextBox::GetText() const noexcept
	{
		return text_;
	}

	void TextBox::SetText(std::string text)
	{
		text_ = std::move(text);
	}

	bool TextBox::IsMultiline() const noexcept
	{
		return multiline_;
	}

	void TextBox::SetMultiline(bool multiline) noexcept
	{
		multiline_ = multiline;
	}

	void TextBox::SetOnChanged(Changed callback)
	{
		changed_ = std::move(callback);
	}

	void TextBox::OnRender()
	{
		const size_t capacity = std::max<size_t>(text_.size() + 1U, 4096U);
		buffer_.assign(capacity, '\0');
		std::copy(text_.begin(), text_.end(), buffer_.begin());
		const ImVec2 size = ResolveControlSize(GetLayout());
		const bool changed = multiline_
			? ImGui::InputTextMultiline(label_.c_str(), buffer_.data(), buffer_.size(), size)
			: ImGui::InputText(label_.c_str(), buffer_.data(), buffer_.size());
		if (changed) {
			text_ = buffer_.data();
			if (changed_) {
				changed_(text_);
			}
		}
	}

	Slider::Slider(std::string label, float value, float minimum, float maximum)
		: label_(std::move(label))
		, value_(value)
		, minimum_(std::min(minimum, maximum))
		, maximum_(std::max(minimum, maximum))
	{
		SetValue(value_);
	}

	const std::string& Slider::GetLabel() const noexcept
	{
		return label_;
	}

	void Slider::SetLabel(std::string label)
	{
		label_ = std::move(label);
	}

	float Slider::GetValue() const noexcept
	{
		return value_;
	}

	void Slider::SetValue(float value)
	{
		value_ = std::clamp(value, minimum_, maximum_);
	}

	void Slider::SetRange(float minimum, float maximum)
	{
		minimum_ = std::min(minimum, maximum);
		maximum_ = std::max(minimum, maximum);
		SetValue(value_);
	}

	void Slider::SetOnChanged(Changed callback)
	{
		changed_ = std::move(callback);
	}

	void Slider::OnRender()
	{
		if (GetLayout().width >= 0.0F) {
			ImGui::SetNextItemWidth(GetLayout().width);
		}
		if (ImGui::SliderFloat(label_.c_str(), &value_, minimum_, maximum_) && changed_) {
			changed_(value_);
		}
	}

	ProgressBar::ProgressBar(float value, std::string overlay)
		: value_(value)
		, overlay_(std::move(overlay))
	{
		SetValue(value);
	}

	float ProgressBar::GetValue() const noexcept
	{
		return value_;
	}

	void ProgressBar::SetValue(float value) noexcept
	{
		value_ = std::clamp(value, 0.0F, 1.0F);
	}

	const std::string& ProgressBar::GetOverlay() const noexcept
	{
		return overlay_;
	}

	void ProgressBar::SetOverlay(std::string overlay)
	{
		overlay_ = std::move(overlay);
	}

	void ProgressBar::OnRender()
	{
		ImGui::ProgressBar(value_, ResolveControlSize(GetLayout()), overlay_.empty() ? nullptr : overlay_.c_str());
	}

	void Separator::OnRender()
	{
		ImGui::Separator();
	}

} // namespace dl::imgui
