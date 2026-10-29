#include "dl_imgui/custom_dialog.h"

#include <algorithm>
#include <utility>

namespace dl::imgui
{

	CustomDialog::CustomDialog(
		std::string title,
		std::string subtitle,
		ImVec2 size,
		ImVec2 pagePadding)
		: Dialog(std::move(title), std::move(subtitle), size)
		, page_(std::make_unique<Page>("", "", pagePadding))
	{
	}

	Page& CustomDialog::GetPage() noexcept
	{
		return *page_;
	}

	const Page& CustomDialog::GetPage() const noexcept
	{
		return *page_;
	}

	void CustomDialog::SetPagePadding(ImVec2 padding) noexcept
	{
		page_->SetPadding(ImVec2(
			std::max(0.0F, padding.x),
			std::max(0.0F, padding.y)));
	}

	void CustomDialog::RenderContent()
	{
		page_->Render();
	}

} // dl::imgui 命名空间结束
