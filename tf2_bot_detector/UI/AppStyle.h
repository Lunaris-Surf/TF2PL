#pragma once

#include <imgui.h>

namespace tf2_bot_detector
{
	/// <summary>
	/// LunarisV "Last Approach" palette (mirrors the tokens in lunaris.surf's stylesheet.css).
	/// Teal glass field, orange HUD accents, magenta as a rare highlight.
	/// </summary>
	namespace LunarisPalette
	{
		constexpr ImVec4 RGBA(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }

		inline constexpr ImVec4 Void       = RGBA(0x02, 0x10, 0x15);
		inline constexpr ImVec4 Deep       = RGBA(0x04, 0x19, 0x1f);
		inline constexpr ImVec4 Panel      = RGBA(4, 28, 34, 0.80f);
		inline constexpr ImVec4 Panel2     = RGBA(8, 44, 52, 0.86f);
		inline constexpr ImVec4 PanelHi    = RGBA(18, 82, 88, 0.55f);
		inline constexpr ImVec4 Well       = RGBA(1, 10, 13, 0.85f);   // input / table wells

		inline constexpr ImVec4 Ink        = RGBA(0xe4, 0xff, 0xfb);
		inline constexpr ImVec4 Muted      = RGBA(0x8f, 0xc5, 0xc0);
		inline constexpr ImVec4 Dim        = RGBA(0x4f, 0x89, 0x84);

		inline constexpr ImVec4 Teal       = RGBA(0x3f, 0xe6, 0xd8);
		inline constexpr ImVec4 TealHi     = RGBA(0xb9, 0xff, 0xf7);
		inline constexpr ImVec4 TealDeep   = RGBA(0x0f, 0x6d, 0x73);
		inline constexpr ImVec4 Orange     = RGBA(0xff, 0x9a, 0x1f);
		inline constexpr ImVec4 OrangeHi   = RGBA(0xff, 0xcf, 0x7a);
		inline constexpr ImVec4 OrangeDeep = RGBA(0xc2, 0x56, 0x0c);
		inline constexpr ImVec4 Magenta    = RGBA(0xff, 0x3f, 0xd8);
		inline constexpr ImVec4 Green      = RGBA(0x8d, 0xff, 0x6a);
		inline constexpr ImVec4 Red        = RGBA(0xff, 0x4f, 0x6d);

		inline constexpr ImVec4 Line       = RGBA(111, 247, 234, 0.22f);
		inline constexpr ImVec4 LineHot    = RGBA(111, 247, 234, 0.55f);

		inline ImVec4 WithAlpha(ImVec4 c, float a) { c.w = a; return c; }
		inline ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
		inline ImU32 U32(const ImVec4& c, float a) { return ImGui::ColorConvertFloat4ToU32(WithAlpha(c, a)); }
	}

	/// <summary>
	/// Applies the LunarisV theme on top of imgui's default dark style.
	/// Called once after ImGui::CreateContext() / ImGui::StyleColorsDark().
	/// (Name kept for the renderer, which calls it.)
	/// </summary>
	inline void ApplyModernDarkTheme()
	{
		using namespace LunarisPalette;
		ImGuiStyle& style = ImGui::GetStyle();

		// HUD look: hard corners everywhere. Cut corners are drawn by hand on panels (see LunarisUI).
		style.WindowRounding = 0.0f;
		style.ChildRounding = 0.0f;
		style.FrameRounding = 0.0f;
		style.PopupRounding = 0.0f;
		style.ScrollbarRounding = 0.0f;
		style.GrabRounding = 0.0f;
		style.TabRounding = 0.0f;

		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.TabBorderSize = 0.0f;
		style.TabBarBorderSize = 1.0f;
		style.TabBarOverlineSize = 2.0f;

		style.WindowPadding = ImVec2(12, 12);
		style.FramePadding = ImVec2(10, 5);
		style.CellPadding = ImVec2(6, 3);
		style.ItemSpacing = ImVec2(8, 6);
		style.ItemInnerSpacing = ImVec2(6, 4);
		style.IndentSpacing = 18.0f;
		style.ScrollbarSize = 12.0f;
		style.GrabMinSize = 12.0f;
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style.SeparatorTextBorderSize = 1.0f;
		style.SeparatorTextPadding = ImVec2(0, 4);

		ImVec4* c = style.Colors;
		c[ImGuiCol_Text]                  = Ink;
		c[ImGuiCol_TextDisabled]          = Dim;
		c[ImGuiCol_WindowBg]              = RGBA(4, 25, 31, 0.98f);
		c[ImGuiCol_ChildBg]               = RGBA(0, 0, 0, 0);
		c[ImGuiCol_PopupBg]               = RGBA(6, 34, 41, 0.98f);
		c[ImGuiCol_Border]                = Line;
		c[ImGuiCol_BorderShadow]          = RGBA(0, 0, 0, 0);

		c[ImGuiCol_FrameBg]               = Well;
		c[ImGuiCol_FrameBgHovered]        = RGBA(63, 230, 216, 0.10f);
		c[ImGuiCol_FrameBgActive]         = RGBA(63, 230, 216, 0.18f);

		c[ImGuiCol_TitleBg]               = Deep;
		c[ImGuiCol_TitleBgActive]         = RGBA(10, 52, 58, 1.0f);
		c[ImGuiCol_TitleBgCollapsed]      = Deep;
		c[ImGuiCol_MenuBarBg]             = RGBA(2, 16, 21, 0.96f);

		c[ImGuiCol_ScrollbarBg]           = RGBA(1, 10, 13, 0.80f);
		c[ImGuiCol_ScrollbarGrab]         = TealDeep;
		c[ImGuiCol_ScrollbarGrabHovered]  = Teal;
		c[ImGuiCol_ScrollbarGrabActive]   = Orange;

		c[ImGuiCol_CheckMark]             = Orange;
		c[ImGuiCol_SliderGrab]            = Teal;
		c[ImGuiCol_SliderGrabActive]      = Orange;

		c[ImGuiCol_Button]                = RGBA(63, 230, 216, 0.14f);
		c[ImGuiCol_ButtonHovered]         = RGBA(63, 230, 216, 0.30f);
		c[ImGuiCol_ButtonActive]          = WithAlpha(OrangeDeep, 0.85f);

		c[ImGuiCol_Header]                = RGBA(63, 230, 216, 0.12f);
		c[ImGuiCol_HeaderHovered]         = RGBA(63, 230, 216, 0.22f);
		c[ImGuiCol_HeaderActive]          = RGBA(63, 230, 216, 0.32f);

		c[ImGuiCol_Separator]             = Line;
		c[ImGuiCol_SeparatorHovered]      = LineHot;
		c[ImGuiCol_SeparatorActive]       = Orange;

		c[ImGuiCol_ResizeGrip]            = RGBA(63, 230, 216, 0.15f);
		c[ImGuiCol_ResizeGripHovered]     = RGBA(63, 230, 216, 0.50f);
		c[ImGuiCol_ResizeGripActive]      = Orange;

		c[ImGuiCol_Tab]                       = RGBA(4, 28, 34, 0.90f);
		c[ImGuiCol_TabHovered]                = RGBA(63, 230, 216, 0.28f);
		c[ImGuiCol_TabSelected]               = RGBA(15, 109, 115, 0.70f);
		c[ImGuiCol_TabSelectedOverline]       = Orange;
		c[ImGuiCol_TabDimmed]                 = RGBA(4, 28, 34, 0.90f);
		c[ImGuiCol_TabDimmedSelected]         = RGBA(15, 109, 115, 0.45f);
		c[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(Orange, 0.5f);

		c[ImGuiCol_DockingPreview]        = RGBA(63, 230, 216, 0.45f);
		c[ImGuiCol_DockingEmptyBg]        = Void;

		c[ImGuiCol_PlotLines]             = Teal;
		c[ImGuiCol_PlotLinesHovered]      = OrangeHi;
		c[ImGuiCol_PlotHistogram]         = Orange;
		c[ImGuiCol_PlotHistogramHovered]  = OrangeHi;

		c[ImGuiCol_TableHeaderBg]         = RGBA(15, 109, 115, 0.35f);
		c[ImGuiCol_TableBorderStrong]     = LineHot;
		c[ImGuiCol_TableBorderLight]      = Line;
		c[ImGuiCol_TableRowBg]            = RGBA(0, 0, 0, 0);
		c[ImGuiCol_TableRowBgAlt]         = RGBA(63, 230, 216, 0.035f);

		c[ImGuiCol_TextSelectedBg]        = WithAlpha(Orange, 0.35f);
		c[ImGuiCol_DragDropTarget]        = Orange;
		c[ImGuiCol_NavHighlight]          = Orange;
		c[ImGuiCol_NavWindowingHighlight] = WithAlpha(TealHi, 0.70f);
		c[ImGuiCol_NavWindowingDimBg]     = RGBA(1, 8, 11, 0.50f);
		c[ImGuiCol_ModalWindowDimBg]      = RGBA(1, 8, 11, 0.75f);
	}
}
