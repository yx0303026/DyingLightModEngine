#include "dl_imgui/message_dialog.h"

#include <utility>

namespace dl::imgui
{

	MessageDialog::MessageDialog(
		std::string title,
		std::string subtitle,
		std::string content,
		ImVec2 size)
		: Dialog(std::move(title), std::move(subtitle), size)
		, content_(std::move(content))
	{
	}

	const std::string& MessageDialog::GetContent() const noexcept
	{
		return content_;
	}

	void MessageDialog::SetContent(std::string content)
	{
		content_ = std::move(content);
	}

	void MessageDialog::RenderContent()
	{
		if (!content_.empty())
			ImGui::TextWrapped("%s", content_.c_str());
	}

} // dl::imgui 命名空间结束
