#pragma once

#include "dl_imgui/dialog.h"

#include <string>

namespace dl::imgui
{

	/* MessageDialog 概览：使用一段可换行文本作为内容区的标准信息弹窗。 */
	class MessageDialog final : public Dialog
	{
	public:
		MessageDialog(
			std::string title = {},
			std::string subtitle = {},
			std::string content = {},
			ImVec2 size = ImVec2(520.0F, 320.0F));

		const std::string& GetContent() const noexcept;
		void SetContent(std::string content);

	protected:
		void RenderContent() override;

	private:
		std::string content_;
	};

} // dl::imgui 命名空间结束
