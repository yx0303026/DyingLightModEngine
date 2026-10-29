#include "dl_imgui/dialog.h"

#include <algorithm>
#include <utility>

namespace dl::imgui
{
	namespace
	{
		/* 弹窗样式概览：使用当前主题颜色，只补充标题和确认按钮的层次。 */
		class DialogStyleScope final
		{
		public:
			DialogStyleScope()
			{
				ImGui::PushStyleVar(
					ImGuiStyleVar_WindowPadding,
					ScaleSize(ImVec2(24.0F, 20.0F)));
				ImGui::PushStyleVar(
					ImGuiStyleVar_ItemSpacing,
					ScaleSize(ImVec2(10.0F, 10.0F)));
				styleCount_ = 2;
			}

			~DialogStyleScope()
			{
				ImGui::PopStyleVar(styleCount_);
			}

		private:
			int styleCount_ = 0;
		};

		/* 弹窗按钮样式概览：把确认操作映射到现有主题的导航高亮色。 */
		class AcceptButtonStyleScope final
		{
		public:
			AcceptButtonStyleScope()
			{
				const ImVec4 accent = ImGui::GetStyle().Colors[ImGuiCol_NavHighlight];
				ImVec4 hovered = accent;
				hovered.x = std::min(1.0F, hovered.x + 0.08F);
				hovered.y = std::min(1.0F, hovered.y + 0.08F);
				hovered.z = std::min(1.0F, hovered.z + 0.08F);
				ImGui::PushStyleColor(ImGuiCol_Button, accent);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
				colorCount_ = 3;
			}

			~AcceptButtonStyleScope()
			{
				ImGui::PopStyleColor(colorCount_);
			}

		private:
			int colorCount_ = 0;
		};

		float ResolveButtonWidth(const std::string& text) noexcept
		{
			const ImGuiStyle& style = ImGui::GetStyle();
			return ImGui::CalcTextSize(text.c_str()).x + style.FramePadding.x * 2.0F;
		}
	}

	Dialog::Dialog(std::string title, std::string subtitle, ImVec2 size)
		: title_(std::move(title))
		, subtitle_(std::move(subtitle))
		, size_(size)
	{
		SetSize(size);
	}

	const std::string& Dialog::GetTitle() const noexcept
	{
		return title_;
	}

	void Dialog::SetTitle(std::string title)
	{
		title_ = std::move(title);
	}

	const std::string& Dialog::GetSubtitle() const noexcept
	{
		return subtitle_;
	}

	void Dialog::SetSubtitle(std::string subtitle)
	{
		subtitle_ = std::move(subtitle);
	}

	ImVec2 Dialog::GetSize() const noexcept
	{
		return size_;
	}

	void Dialog::SetSize(ImVec2 size) noexcept
	{
		size_.x = std::max(240.0F, size.x);
		size_.y = std::max(160.0F, size.y);
	}

	bool Dialog::IsOpen() const noexcept
	{
		return openRequested_ || (popupOpen_ && !closeRequested_);
	}

	void Dialog::Show() noexcept
	{
		openRequested_ = true;
		closeRequested_ = false;
		resultNotified_ = false;
		lastResult_ = DialogResult::None;
	}

	void Dialog::Hide() noexcept
	{
		openRequested_ = false;
		closeRequested_ = popupOpen_;
	}

	void Dialog::Open() noexcept
	{
		Show();
	}

	void Dialog::Close() noexcept
	{
		Hide();
	}

	DialogResult Dialog::GetLastResult() const noexcept
	{
		return lastResult_;
	}

	bool Dialog::IsCloseButtonVisible() const noexcept
	{
		return closeButtonVisible_;
	}

	void Dialog::SetCloseButtonVisible(bool visible) noexcept
	{
		closeButtonVisible_ = visible;
	}

	bool Dialog::IsAcceptButtonVisible() const noexcept
	{
		return acceptButtonVisible_;
	}

	void Dialog::SetAcceptButtonVisible(bool visible) noexcept
	{
		acceptButtonVisible_ = visible;
	}

	const std::string& Dialog::GetCloseButtonText() const noexcept
	{
		return closeButtonText_;
	}

	void Dialog::SetCloseButtonText(std::string text)
	{
		closeButtonText_ = std::move(text);
	}

	const std::string& Dialog::GetAcceptButtonText() const noexcept
	{
		return acceptButtonText_;
	}

	void Dialog::SetAcceptButtonText(std::string text)
	{
		acceptButtonText_ = std::move(text);
	}

	void Dialog::SetOnClosed(Callback callback)
	{
		closed_ = std::move(callback);
	}

	void Dialog::SetOnAccepted(Callback callback)
	{
		accepted_ = std::move(callback);
	}

	void Dialog::SetOnClose(Callback callback)
	{
		SetOnClosed(std::move(callback));
	}

	void Dialog::SetOnAccept(Callback callback)
	{
		SetOnAccepted(std::move(callback));
	}

	void Dialog::OnRender()
	{
		if (!openRequested_ && !popupOpen_ && !closeRequested_)
			return;

		const bool wasOpen = popupOpen_;
		if (openRequested_ && !popupOpen_)
		{
			ImGui::SetNextWindowSize(ScaleSize(size_), ImGuiCond_Appearing);
			ImGui::OpenPopup(kPopupId);
		}

		const bool visible = ImGui::BeginPopupModal(
			kPopupId,
			nullptr,
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoTitleBar);
		if (!visible)
		{
			HandlePopupClosed(wasOpen);
			return;
		}

		popupOpen_ = true;
		DialogStyleScope styleScope;
		if (closeRequested_)
		{
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			popupOpen_ = false;
			closeRequested_ = false;
			return;
		}

		RenderHeader();
		RenderContentArea();
		RenderFooter();
		ImGui::EndPopup();
	}

	void Dialog::RenderHeader()
	{
		if (!title_.empty())
		{
			ImGui::TextColored(
				ImGui::GetStyle().Colors[ImGuiCol_NavHighlight],
				"%s",
				title_.c_str());
		}
		if (!subtitle_.empty())
		{
			ImGui::TextDisabled("%s", subtitle_.c_str());
		}
		if (!title_.empty() || !subtitle_.empty())
			ImGui::Separator();
	}

	void Dialog::RenderContentArea()
	{
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float contentHeight = std::max(0.0F, available.y - GetFooterHeight());
		if (!ImGui::BeginChild("##dialog_content", ImVec2(0.0F, contentHeight), false))
		{
			ImGui::EndChild();
			return;
		}
		RenderContent();
		ImGui::EndChild();
	}

	void Dialog::RenderFooter()
	{
		if (!HasFooter())
			return;

		ImGui::Separator();
		const ImGuiStyle& style = ImGui::GetStyle();
		const float spacing = style.ItemSpacing.x;
		float totalWidth = 0.0F;
		if (closeButtonVisible_)
			totalWidth += ResolveButtonWidth(closeButtonText_);
		if (acceptButtonVisible_)
		{
			if (closeButtonVisible_)
				totalWidth += spacing;
			totalWidth += ResolveButtonWidth(acceptButtonText_);
		}

		const float availableWidth = ImGui::GetContentRegionAvail().x;
		const float trailingSpace = std::max(0.0F, availableWidth - totalWidth);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + trailingSpace);

		if (closeButtonVisible_ && ImGui::Button(closeButtonText_.c_str()))
			Complete(DialogResult::Closed, true);

		if (closeButtonVisible_ && acceptButtonVisible_)
			ImGui::SameLine();

		if (acceptButtonVisible_)
		{
			AcceptButtonStyleScope styleScope;
			if (ImGui::Button(acceptButtonText_.c_str()))
				Complete(DialogResult::Accepted, true);
		}
	}

	void Dialog::Complete(DialogResult result, bool closePopup)
	{
		if (resultNotified_)
			return;

		resultNotified_ = true;
		lastResult_ = result;
		openRequested_ = false;
		closeRequested_ = closePopup;
		popupOpen_ = false;
		if (closePopup)
			ImGui::CloseCurrentPopup();

		Callback& callback = result == DialogResult::Accepted ? accepted_ : closed_;
		if (callback)
			callback();
	}

	void Dialog::HandlePopupClosed(bool wasOpen)
	{
		popupOpen_ = false;
		if (wasOpen && openRequested_ && !closeRequested_)
		{
			Complete(DialogResult::Closed, false);
			return;
		}
		if (!openRequested_)
			closeRequested_ = false;
	}

	bool Dialog::HasFooter() const noexcept
	{
		return closeButtonVisible_ || acceptButtonVisible_;
	}

	float Dialog::GetFooterHeight() const noexcept
	{
		if (!HasFooter())
			return 0.0F;
		const ImGuiStyle& style = ImGui::GetStyle();
		return ImGui::GetFrameHeight() + style.ItemSpacing.y + style.SeparatorTextPadding.y;
	}

} // dl::imgui 命名空间结束
