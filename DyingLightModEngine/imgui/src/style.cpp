#include "dl_imgui/style.h"

namespace dl::imgui {
	namespace {

		void SetSidebarColors(ImGuiStyle& style) noexcept
		{
			style.Colors[ImGuiCol_Text] = ImVec4(0.87F, 0.90F, 0.92F, 1.00F);
			style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.40F, 0.45F, 0.48F, 1.00F);
			style.Colors[ImGuiCol_WindowBg] = ImVec4(0.025F, 0.040F, 0.050F, 0.98F);
			style.Colors[ImGuiCol_ChildBg] = ImVec4(0.020F, 0.032F, 0.042F, 0.96F);
			style.Colors[ImGuiCol_PopupBg] = ImVec4(0.035F, 0.050F, 0.060F, 0.99F);
			style.Colors[ImGuiCol_Border] = ImVec4(0.12F, 0.16F, 0.18F, 0.65F);
			style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00F, 0.00F, 0.00F, 0.00F);
			style.Colors[ImGuiCol_FrameBg] = ImVec4(0.075F, 0.095F, 0.105F, 1.00F);
			style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.105F, 0.130F, 0.145F, 1.00F);
			style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.125F, 0.155F, 0.170F, 1.00F);
			style.Colors[ImGuiCol_TitleBg] = ImVec4(0.020F, 0.032F, 0.042F, 1.00F);
			style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.025F, 0.040F, 0.050F, 1.00F);
			style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.020F, 0.032F, 0.042F, 0.75F);
			style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.030F, 0.045F, 0.055F, 1.00F);
			style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.015F, 0.025F, 0.030F, 0.75F);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18F, 0.23F, 0.25F, 0.80F);
			style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25F, 0.31F, 0.33F, 0.90F);
			style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.31F, 0.38F, 0.40F, 1.00F);
			style.Colors[ImGuiCol_CheckMark] = ImVec4(0.34F, 0.70F, 0.95F, 1.00F);
			style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.28F, 0.62F, 0.86F, 1.00F);
			style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.38F, 0.73F, 0.96F, 1.00F);
			style.Colors[ImGuiCol_Button] = ImVec4(0.075F, 0.095F, 0.105F, 1.00F);
			style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.12F, 0.16F, 0.18F, 1.00F);
			style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.16F, 0.21F, 0.23F, 1.00F);
			style.Colors[ImGuiCol_Header] = ImVec4(0.15F, 0.19F, 0.23F, 0.96F);
			style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.19F, 0.24F, 0.28F, 1.00F);
			style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.22F, 0.29F, 0.33F, 1.00F);
			style.Colors[ImGuiCol_Separator] = ImVec4(0.12F, 0.16F, 0.18F, 0.65F);
			style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.25F, 0.48F, 0.62F, 0.78F);
			style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.32F, 0.62F, 0.80F, 1.00F);
			style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.20F, 0.42F, 0.55F, 0.25F);
			style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.28F, 0.60F, 0.78F, 0.67F);
			style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.35F, 0.72F, 0.92F, 0.95F);
			style.Colors[ImGuiCol_Tab] = ImVec4(0.045F, 0.065F, 0.075F, 1.00F);
			style.Colors[ImGuiCol_TabHovered] = ImVec4(0.16F, 0.25F, 0.30F, 1.00F);
			style.Colors[ImGuiCol_TabActive] = ImVec4(0.12F, 0.20F, 0.25F, 1.00F);
			style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.035F, 0.050F, 0.060F, 1.00F);
			style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.08F, 0.13F, 0.16F, 1.00F);
			style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.30F, 0.64F, 0.86F, 1.00F);
			style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00F, 0.00F, 0.00F, 0.62F);
		}

		void SetSidebarMetrics(ImGuiStyle& style) noexcept
		{
			style.WindowPadding = ImVec2(14.0F, 12.0F);
			style.FramePadding = ImVec2(10.0F, 7.0F);
			style.CellPadding = ImVec2(8.0F, 5.0F);
			style.ItemSpacing = ImVec2(8.0F, 8.0F);
			style.ItemInnerSpacing = ImVec2(8.0F, 5.0F);
			style.TouchExtraPadding = ImVec2(0.0F, 0.0F);
			style.IndentSpacing = 18.0F;
			style.ScrollbarSize = 10.0F;
			style.GrabMinSize = 10.0F;
			style.WindowBorderSize = 1.0F;
			style.ChildBorderSize = 0.0F;
			style.PopupBorderSize = 1.0F;
			style.FrameBorderSize = 0.0F;
			style.TabBorderSize = 0.0F;
			style.WindowRounding = 6.0F;
			style.ChildRounding = 5.0F;
			style.FrameRounding = 5.0F;
			style.PopupRounding = 5.0F;
			style.ScrollbarRounding = 5.0F;
			style.GrabRounding = 5.0F;
			style.LogSliderDeadzone = 4.0F;
			style.TabRounding = 5.0F;
			style.WindowTitleAlign = ImVec2(0.0F, 0.5F);
			style.WindowMenuButtonPosition = ImGuiDir_None;
			style.ColorButtonPosition = ImGuiDir_Right;
			style.ButtonTextAlign = ImVec2(0.5F, 0.5F);
			style.SelectableTextAlign = ImVec2(0.0F, 0.5F);
		}

	} // namespace

	void ApplyStyle(StylePreset preset) noexcept
	{
		if (ImGui::GetCurrentContext() == nullptr) {
			return;
		}

		switch (preset) {
		case StylePreset::Sidebar:
		default:
			ApplyDefaultStyle();
			break;
		}
	}

	void ApplyDefaultStyle() noexcept
	{
		if (ImGui::GetCurrentContext() == nullptr) {
			return;
		}

		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::StyleColorsDark(&style);
		SetSidebarColors(style);
		SetSidebarMetrics(style);
	}

} // namespace dl::imgui
