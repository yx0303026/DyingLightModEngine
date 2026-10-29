#include "dl_imgui/style.h"

namespace dl::imgui {
	namespace {

		void SetSidebarColors(ImGuiStyle& style) noexcept
		{
			style.Colors[ImGuiCol_Text] = ImVec4(0.87F, 0.90F, 0.92F, 1.00F);
			style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.40F, 0.45F, 0.48F, 1.00F);
			style.Colors[ImGuiCol_WindowBg] = ImVec4(0.075F, 0.090F, 0.130F, 0.98F);
			style.Colors[ImGuiCol_ChildBg] = ImVec4(0.085F, 0.100F, 0.145F, 0.98F);
			style.Colors[ImGuiCol_PopupBg] = ImVec4(0.105F, 0.120F, 0.165F, 0.99F);
			style.Colors[ImGuiCol_Border] = ImVec4(0.18F, 0.205F, 0.270F, 0.65F);
			style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00F, 0.00F, 0.00F, 0.00F);
			style.Colors[ImGuiCol_FrameBg] = ImVec4(0.145F, 0.165F, 0.225F, 1.00F);
			style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.180F, 0.205F, 0.275F, 1.00F);
			style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.220F, 0.250F, 0.325F, 1.00F);
			style.Colors[ImGuiCol_TitleBg] = ImVec4(0.075F, 0.090F, 0.130F, 1.00F);
			style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.085F, 0.100F, 0.145F, 1.00F);
			style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.075F, 0.090F, 0.130F, 0.75F);
			style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.095F, 0.110F, 0.155F, 1.00F);
			style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.055F, 0.065F, 0.095F, 0.75F);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.250F, 0.285F, 0.370F, 0.80F);
			style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.330F, 0.375F, 0.470F, 0.90F);
			style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.400F, 0.455F, 0.560F, 1.00F);
			style.Colors[ImGuiCol_CheckMark] = ImVec4(0.300F, 0.760F, 0.960F, 1.00F);
			style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.270F, 0.680F, 0.900F, 1.00F);
			style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.360F, 0.780F, 0.980F, 1.00F);
			style.Colors[ImGuiCol_Button] = ImVec4(0.145F, 0.165F, 0.225F, 1.00F);
			style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.190F, 0.220F, 0.290F, 1.00F);
			style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.230F, 0.270F, 0.350F, 1.00F);
			style.Colors[ImGuiCol_Header] = ImVec4(0.155F, 0.180F, 0.245F, 0.98F);
			style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.190F, 0.220F, 0.290F, 1.00F);
			style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.225F, 0.265F, 0.345F, 1.00F);
			style.Colors[ImGuiCol_Separator] = ImVec4(0.180F, 0.205F, 0.270F, 0.65F);
			style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.260F, 0.540F, 0.700F, 0.78F);
			style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.320F, 0.680F, 0.860F, 1.00F);
			style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.220F, 0.460F, 0.620F, 0.25F);
			style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.300F, 0.640F, 0.820F, 0.67F);
			style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.380F, 0.760F, 0.940F, 0.95F);
			style.Colors[ImGuiCol_Tab] = ImVec4(0.100F, 0.120F, 0.170F, 1.00F);
			style.Colors[ImGuiCol_TabHovered] = ImVec4(0.190F, 0.280F, 0.360F, 1.00F);
			style.Colors[ImGuiCol_TabActive] = ImVec4(0.150F, 0.240F, 0.315F, 1.00F);
			style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.085F, 0.100F, 0.145F, 1.00F);
			style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.120F, 0.180F, 0.240F, 1.00F);
			style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.300F, 0.760F, 0.960F, 1.00F);
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
			style.WindowRounding = 7.0F;
			style.ChildRounding = 7.0F;
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

	} // 匿名命名空间

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

} // dl::imgui 命名空间
