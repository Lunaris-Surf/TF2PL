#pragma once

#include <imgui.h>

namespace tf2_bot_detector
{
	/// <summary>
	/// Applies a modern, slightly-rounded dark theme on top of imgui's default dark style.
	/// Called once after ImGui::CreateContext() / ImGui::StyleColorsDark().
	/// </summary>
	inline void ApplyModernDarkTheme()
	{
		ImGuiStyle& style = ImGui::GetStyle();

		style.WindowRounding = 6.0f;
		style.ChildRounding = 4.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 4.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding = 4.0f;
		style.TabRounding = 4.0f;

		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;

		style.WindowPadding = ImVec2(8, 8);
		style.FramePadding = ImVec2(8, 4);
		style.CellPadding = ImVec2(4, 2);
		style.ItemSpacing = ImVec2(8, 6);
		style.ItemInnerSpacing = ImVec2(6, 4);
		style.ScrollbarSize = 12.0f;
		style.GrabMinSize = 10.0f;
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

		style.Colors[ImGuiCol_Text] = ImVec4(0.86f, 0.87f, 0.90f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.46f, 0.50f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.07f, 0.08f, 0.96f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.20f, 0.23f, 1.00f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.25f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.07f, 0.08f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.33f, 0.38f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.44f, 0.50f, 1.00f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.62f, 0.95f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.56f, 0.88f, 0.80f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.62f, 0.95f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.28f, 0.42f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.44f, 0.68f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.14f, 0.20f, 0.29f, 0.85f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.32f, 0.48f, 0.80f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.42f, 0.64f, 0.80f);
		style.Colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
		style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.24f, 0.44f, 0.68f, 0.80f);
		style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.30f, 0.62f, 0.95f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.56f, 0.88f, 0.20f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.56f, 0.88f, 0.60f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.30f, 0.62f, 0.95f, 0.90f);
		style.Colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.13f, 0.16f, 1.00f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.34f, 0.50f, 0.80f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(0.17f, 0.28f, 0.44f, 1.00f);
		style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.13f, 0.16f, 1.00f);
		style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.13f, 0.21f, 0.34f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.26f, 0.56f, 0.88f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.26f, 0.56f, 0.88f, 0.90f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.30f, 0.62f, 0.95f, 1.00f);
		style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.11f, 0.13f, 0.16f, 1.00f);
		style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
		style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
		style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.56f, 0.88f, 0.35f);
		style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
		style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.30f, 0.62f, 0.95f, 1.00f);
		style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);
	}
}