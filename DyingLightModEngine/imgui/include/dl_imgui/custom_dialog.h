#pragma once

#include "dl_imgui/dialog.h"
#include "dl_imgui/navigation.h"

#include <memory>
#include <string>

namespace dl::imgui
{

	/* CustomDialog 概览：拥有一个可自由组合控件的 Page 内容页，复用标准弹窗页脚。 */
	class CustomDialog final : public Dialog
	{
	public:
		CustomDialog(
			std::string title = {},
			std::string subtitle = {},
			ImVec2 size = ImVec2(640.0F, 440.0F),
			ImVec2 pagePadding = ImVec2(16.0F, 16.0F));

		Page& GetPage() noexcept;
		const Page& GetPage() const noexcept;
		void SetPagePadding(ImVec2 padding) noexcept;

	protected:
		void RenderContent() override;

	private:
		std::unique_ptr<Page> page_;
	};

} // dl::imgui 命名空间结束
