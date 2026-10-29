#pragma once

#include "dl_imgui/ui.h"

#include <functional>
#include <string>

namespace dl::imgui
{

	/* Dialog 基类概览：提供模态弹窗状态、标题区、内容区、页脚按钮和结果回调。 */
	enum class DialogResult
	{
		None,
		Closed,
		Accepted
	};

	/* Dialog 基类概览：派生类只负责内容区，通用弹窗行为由此类统一维护。 */
	class Dialog : public UIElement
	{
	public:
		using Callback = std::function<void()>;

		Dialog(
			std::string title = {},
			std::string subtitle = {},
			ImVec2 size = ImVec2(520.0F, 320.0F));
		~Dialog() override = default;
		Dialog(const Dialog&) = delete;
		Dialog& operator=(const Dialog&) = delete;

		const std::string& GetTitle() const noexcept;
		void SetTitle(std::string title);
		const std::string& GetSubtitle() const noexcept;
		void SetSubtitle(std::string subtitle);
		ImVec2 GetSize() const noexcept;
		void SetSize(ImVec2 size) noexcept;

		bool IsOpen() const noexcept;
		void Show() noexcept;
		void Hide() noexcept;
		void Open() noexcept;
		void Close() noexcept;
		DialogResult GetLastResult() const noexcept;

		bool IsCloseButtonVisible() const noexcept;
		void SetCloseButtonVisible(bool visible) noexcept;
		bool IsAcceptButtonVisible() const noexcept;
		void SetAcceptButtonVisible(bool visible) noexcept;
		const std::string& GetCloseButtonText() const noexcept;
		void SetCloseButtonText(std::string text);
		const std::string& GetAcceptButtonText() const noexcept;
		void SetAcceptButtonText(std::string text);

		void SetOnClosed(Callback callback);
		void SetOnAccepted(Callback callback);
		void SetOnClose(Callback callback);
		void SetOnAccept(Callback callback);

	protected:
		void OnRender() override;
		virtual void RenderContent() = 0;

	private:
		static constexpr const char* kPopupId = "##dl_imgui_dialog";

		void RenderHeader();
		void RenderContentArea();
		void RenderFooter();
		void Complete(DialogResult result, bool closePopup);
		void HandlePopupClosed(bool wasOpen);
		bool HasFooter() const noexcept;
		float GetFooterHeight() const noexcept;

		std::string title_;
		std::string subtitle_;
		ImVec2 size_{};
		std::string closeButtonText_ = "Close";
		std::string acceptButtonText_ = "Accept";
		Callback closed_;
		Callback accepted_;
		bool closeButtonVisible_ = true;
		bool acceptButtonVisible_ = true;
		bool openRequested_ = false;
		bool popupOpen_ = false;
		bool closeRequested_ = false;
		bool resultNotified_ = false;
		DialogResult lastResult_ = DialogResult::None;
	};

} // dl::imgui 命名空间结束
