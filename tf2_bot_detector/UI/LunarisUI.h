#pragma once

#include "AppStyle.h"

#include <imgui.h>

#include <string_view>

/// <summary>
/// Hand-drawn pieces of the LunarisV HUD look that plain ImGui styling can't express:
/// glass panels with cut corners, orange micro-labels, skewed buttons, toggle chips,
/// 7-segment counters and the background field. Everything is static (no animation)
/// so it costs next to nothing per frame.
/// </summary>
namespace tf2_bot_detector::LunarisUI
{
	struct FontSet
	{
		ImFont* m_Body = nullptr;      // Chakra Petch 16px (+ unifont glyphs for names)
		ImFont* m_BodyBold = nullptr;  // Chakra Petch 600 16px
		ImFont* m_Label = nullptr;     // Chakra Petch 600 italic 11px (micro labels)
		ImFont* m_Display = nullptr;   // Michroma 12px (buttons, nav)
		ImFont* m_DisplayTitle = nullptr; // Michroma 15px (panel titles)
		ImFont* m_DisplayLarge = nullptr; // Michroma 20px (window titles)
		ImFont* m_Mono = nullptr;      // Share Tech Mono 15px (steam ids, timestamps)
		ImFont* m_Segment = nullptr;   // DSEG7 Classic bold italic 20px (counters)
	};

	FontSet& Fonts();

	/// <summary>
	/// Fill rect with the void/deep field, a soft vignette, faint diagonal streaks and the
	/// orange hatch stripe on the left edge (bg-field / bg-stripes on the site).
	/// </summary>
	void DrawBackground(ImDrawList* dl, ImVec2 min, ImVec2 max);

	/// <summary>
	/// The ".glass" panel: cut top-right and bottom-left corners, teal gradient fill,
	/// 1px line border, top highlight, orange tab at the top-left.
	/// </summary>
	void DrawGlassPanel(ImDrawList* dl, ImVec2 min, ImVec2 max, float cut = 14.0f, bool orangeTab = true, bool opaque = false);

	/// <summary>
	/// Glass panel child window with a micro-label header ("COMMS // CHAT").
	/// Always call EndPanel() (like EndChild).
	/// </summary>
	bool BeginPanel(const char* id, std::string_view label, std::string_view title, ImVec2 size);
	void EndPanel();

	/// <summary>
	/// Top-level window drawn as an opaque glass panel with a HUD header instead of imgui's title bar.
	/// The title is the window name up to "##". Always call ImGui::End() afterwards.
	/// </summary>
	bool BeginWindow(const char* name, std::string_view label, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);

	/// <summary>
	/// Orange italic uppercase label with a skewed bar in front (".micro-label").
	/// </summary>
	void MicroLabel(std::string_view text, const ImVec4& color = LunarisPalette::Orange, bool bar = true);

	/// <summary>
	/// Text drawn with extra letter spacing (the site leans on tracking a lot).
	/// </summary>
	void SpacedText(ImFont* font, std::string_view text, const ImVec4& color, float letterSpacing);
	ImVec2 CalcSpacedTextSize(ImFont* font, std::string_view text, float letterSpacing);
	void AddSpacedText(ImDrawList* dl, ImFont* font, ImVec2 pos, const ImVec4& color, std::string_view text, float letterSpacing);

	enum class ButtonStyle
	{
		Primary,   // orange
		Secondary, // teal
		Ghost,     // outline
	};

	/// <summary>
	/// Skewed (parallelogram) button with uppercase display type.
	/// </summary>
	bool SkewButton(const char* label, ButtonStyle style = ButtonStyle::Secondary, ImVec2 size = ImVec2(0, 0));

	/// <summary>
	/// Small skewed on/off chip. On: teal fill with a green diamond. Off: orange outline,
	/// so disabled moderation features stand out (same meaning as the old orange labels).
	/// </summary>
	/// warnWhenOff=false draws "off" as a neutral outline instead (for non-moderation toggles).
	bool ToggleChip(const char* label, bool* value, bool warnWhenOff = true);

	/// <summary>
	/// "PLAYERS [ 24 ]" style readout: micro label + 7-segment number.
	/// </summary>
	void SegmentCounter(std::string_view label, int value, int digits = 2, const ImVec4& color = LunarisPalette::TealHi);

	/// <summary>
	/// Thin horizontal HUD rule: orange lead segment then a teal line.
	/// </summary>
	void Rule(float width = 0.0f);
}
