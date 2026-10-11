#include "LunarisUI.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace tf2_bot_detector;
using namespace tf2_bot_detector::LunarisPalette;

namespace
{
	ImVec4 Lerp(const ImVec4& a, const ImVec4& b, float t)
	{
		return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
	}

	/// <summary>
	/// convex polygon fill with a 2-stop linear gradient from `from` to `to` along `dir`.
	/// (ImDrawList has no gradient polygons; this writes the vertices directly.)
	/// </summary>
	void FillConvexGradient(ImDrawList* dl, const ImVec2* pts, int count,
		const ImVec4& from, const ImVec4& to, ImVec2 dirStart, ImVec2 dirEnd)
	{
		if (count < 3)
			return;

		const ImVec2 d = { dirEnd.x - dirStart.x, dirEnd.y - dirStart.y };
		const float len2 = std::max(1.0f, d.x * d.x + d.y * d.y);
		const ImVec2 uv = dl->_Data->TexUvWhitePixel;

		dl->PrimReserve((count - 2) * 3, count);
		const ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
		for (int i = 0; i < count; i++)
		{
			const float t = std::clamp(((pts[i].x - dirStart.x) * d.x + (pts[i].y - dirStart.y) * d.y) / len2, 0.0f, 1.0f);
			dl->PrimWriteVtx(pts[i], uv, ImGui::ColorConvertFloat4ToU32(Lerp(from, to, t)));
		}
		for (int i = 2; i < count; i++)
		{
			dl->PrimWriteIdx(base);
			dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + i - 1));
			dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + i));
		}
	}

	float FontPx(ImFont* font)
	{
		return font->FontSize * ImGui::GetIO().FontGlobalScale;
	}

	ImFont* OrCurrent(ImFont* font)
	{
		return font ? font : ImGui::GetFont();
	}

	std::string Upper(std::string_view s)
	{
		std::string out(s);
		for (char& ch : out)
		{
			if (ch >= 'a' && ch <= 'z')
				ch = static_cast<char>(ch - 'a' + 'A');
		}
		return out;
	}

	void AddParallelogram(ImDrawList* dl, ImVec2 min, ImVec2 max, float skew, ImU32 col)
	{
		const ImVec2 pts[4] = { { min.x + skew, min.y }, { max.x, min.y }, { max.x - skew, max.y }, { min.x, max.y } };
		dl->AddConvexPolyFilled(pts, 4, col);
	}

	void StrokeParallelogram(ImDrawList* dl, ImVec2 min, ImVec2 max, float skew, ImU32 col, float thickness = 1.0f)
	{
		const ImVec2 pts[4] = { { min.x + skew, min.y }, { max.x, min.y }, { max.x - skew, max.y }, { min.x, max.y } };
		dl->AddPolyline(pts, 4, col, ImDrawFlags_Closed, thickness);
	}

	void AddDiamond(ImDrawList* dl, ImVec2 center, float r, ImU32 col, bool filled)
	{
		const ImVec2 pts[4] = { { center.x, center.y - r }, { center.x + r, center.y }, { center.x, center.y + r }, { center.x - r, center.y } };
		if (filled)
			dl->AddConvexPolyFilled(pts, 4, col);
		else
			dl->AddPolyline(pts, 4, col, ImDrawFlags_Closed, 1.0f);
	}

	void DrawSpacedText(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, std::string_view text, float spacing)
	{
		const float size = FontPx(font);
		const char* s = text.data();
		const char* end = text.data() + text.size();
		while (s < end)
		{
			unsigned int c = 0;
			const int len = ImTextCharFromUtf8(&c, s, end);
			if (len <= 0)
				break;
			dl->AddText(font, size, pos, col, s, s + len);
			pos.x += font->CalcTextSizeA(size, FLT_MAX, 0, s, s + len).x + spacing;
			s += len;
		}
	}
}

LunarisUI::FontSet& LunarisUI::Fonts()
{
	static FontSet s_Fonts;
	return s_Fonts;
}

ImVec2 LunarisUI::CalcSpacedTextSize(ImFont* font, std::string_view text, float letterSpacing)
{
	font = OrCurrent(font);
	const float size = FontPx(font);
	ImVec2 total = font->CalcTextSizeA(size, FLT_MAX, 0, text.data(), text.data() + text.size());

	size_t glyphs = 0;
	const char* s = text.data();
	const char* end = text.data() + text.size();
	while (s < end)
	{
		unsigned int c = 0;
		const int len = ImTextCharFromUtf8(&c, s, end);
		if (len <= 0)
			break;
		glyphs++;
		s += len;
	}

	if (glyphs > 1)
		total.x += letterSpacing * float(glyphs - 1);
	return total;
}

void LunarisUI::AddSpacedText(ImDrawList* dl, ImFont* font, ImVec2 pos, const ImVec4& color, std::string_view text, float letterSpacing)
{
	DrawSpacedText(dl, OrCurrent(font), pos, U32(color), text, letterSpacing);
}

void LunarisUI::SpacedText(ImFont* font, std::string_view text, const ImVec4& color, float letterSpacing)
{
	font = OrCurrent(font);
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 size = CalcSpacedTextSize(font, text, letterSpacing);
	DrawSpacedText(ImGui::GetWindowDrawList(), font, pos, U32(color), text, letterSpacing);
	ImGui::Dummy(size);
}

void LunarisUI::DrawBackground(ImDrawList* dl, ImVec2 min, ImVec2 max)
{
	const float w = max.x - min.x;
	const float h = max.y - min.y;
	if (w <= 0 || h <= 0)
		return;

	// base field: void at the top fading into deep teal
	dl->AddRectFilledMultiColor(min, max, U32(Void), U32(Void), U32(Deep), U32(Deep));

	// soft teal glow, upper right of centre (radial-gradient on the site)
	{
		const ImVec2 c = { min.x + w * 0.62f, min.y + h * 0.38f };
		const float maxR = std::max(w, h) * 0.55f;
		constexpr int rings = 7;
		for (int i = 0; i < rings; i++)
		{
			const float r = maxR * (1.0f - float(i) / rings);
			dl->AddCircleFilled(c, r, U32(TealDeep, 0.035f), 48);
		}
	}

	// faint diagonal streaks (bg-field.svg), fixed positions so nothing moves per frame
	struct Streak { float x0, y0, x1, y1; bool orange; float alpha; float thick; };
	static constexpr Streak streaks[] =
	{
		{ 0.05f, 0.30f, 0.30f, 0.10f, true,  0.16f, 2.0f },
		{ 0.38f, 0.92f, 0.62f, 0.72f, true,  0.12f, 2.0f },
		{ 0.70f, 0.20f, 0.95f, 0.02f, true,  0.14f, 3.0f },
		{ 0.15f, 0.75f, 0.45f, 0.52f, false, 0.10f, 1.0f },
		{ 0.55f, 0.45f, 0.88f, 0.20f, false, 0.08f, 1.0f },
		{ 0.80f, 0.95f, 1.00f, 0.80f, false, 0.10f, 1.0f },
	};
	for (const auto& s : streaks)
	{
		dl->AddLine({ min.x + w * s.x0, min.y + h * s.y0 }, { min.x + w * s.x1, min.y + h * s.y1 },
			U32(s.orange ? Orange : Teal, s.alpha), s.thick);
	}

	// darken the edges (vignette)
	{
		const ImU32 clear = U32(Void, 0.0f);
		const ImU32 edge = U32(RGBA(1, 8, 11), 0.55f);
		const float band = std::min(w, h) * 0.18f;
		dl->AddRectFilledMultiColor(min, { max.x, min.y + band }, edge, edge, clear, clear);
		dl->AddRectFilledMultiColor({ min.x, max.y - band }, max, clear, clear, edge, edge);
		dl->AddRectFilledMultiColor(min, { min.x + band, max.y }, edge, clear, clear, edge);
		dl->AddRectFilledMultiColor({ max.x - band, min.y }, max, clear, edge, edge, clear);
	}

	// orange hatch stripe down the left edge, fading out at both ends
	{
		constexpr float stripeW = 18.0f;
		dl->PushClipRect(min, { min.x + stripeW, max.y }, true);
		for (float y = min.y - stripeW; y < max.y + stripeW; y += 12.0f)
		{
			const float t = (y - min.y) / h;
			const float fade = std::clamp(std::min(t, 1.0f - t) * 3.0f, 0.0f, 1.0f);
			if (fade <= 0.0f)
				continue;

			const ImVec2 pts[4] = { { min.x, y + 5 }, { min.x, y }, { min.x + stripeW, y - stripeW * 0.7f }, { min.x + stripeW, y - stripeW * 0.7f + 5 } };
			dl->AddConvexPolyFilled(pts, 4, U32(Orange, 0.26f * fade));
		}
		dl->PopClipRect();
	}
}

void LunarisUI::DrawGlassPanel(ImDrawList* dl, ImVec2 min, ImVec2 max, float cut, bool orangeTab, bool opaque)
{
	cut = std::min(cut, std::min(max.x - min.x, max.y - min.y) * 0.5f);

	const ImVec2 pts[6] =
	{
		{ min.x, min.y },
		{ max.x - cut, min.y },
		{ max.x, min.y + cut },
		{ max.x, max.y },
		{ min.x + cut, max.y },
		{ min.x, max.y - cut },
	};

	// opaque for top-level windows and the color-keyed game overlay (any transparency there would
	// blend with the magenta key color)
	FillConvexGradient(dl, pts, 6, RGBA(8, 44, 52, opaque ? 1.0f : 0.88f), RGBA(3, 20, 25, opaque ? 1.0f : 0.90f), min, max);
	dl->AddPolyline(pts, 6, U32(Line), ImDrawFlags_Closed, 1.0f);

	// top inner highlight and the hot line across the cut corner
	dl->AddLine({ min.x + 1, min.y + 1.5f }, { max.x - cut, min.y + 1.5f }, U32(RGBA(185, 255, 247, 0.16f)));
	dl->AddLine({ max.x - cut, min.y }, { max.x, min.y + cut }, U32(LineHot), 1.0f);

	if (orangeTab)
		dl->AddRectFilled(min, { min.x + std::min(54.0f, (max.x - min.x) * 0.3f), min.y + 3 }, U32(Orange));
}

bool LunarisUI::BeginPanel(const char* id, std::string_view label, std::string_view title, ImVec2 size)
{
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	if (size.x <= 0)
		size.x = std::max(1.0f, avail.x + size.x);
	if (size.y <= 0)
		size.y = std::max(1.0f, avail.y + size.y);

	const ImVec2 pos = ImGui::GetCursorScreenPos();
	DrawGlassPanel(ImGui::GetWindowDrawList(), pos, { pos.x + size.x, pos.y + size.y });

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 14, 12 });
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
	const bool open = ImGui::BeginChild(id, size, ImGuiChildFlags_AlwaysUseWindowPadding,
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar(2);

	if (open && (!label.empty() || !title.empty()))
	{
		if (!label.empty())
		{
			MicroLabel(label);
			if (!title.empty())
				ImGui::SameLine(0, 10);
		}
		if (!title.empty())
		{
			ImGui::PushFont(OrCurrent(Fonts().m_DisplayTitle));
			ImGui::PushStyleColor(ImGuiCol_Text, TealHi);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(title.data(), title.data() + title.size());
			ImGui::PopStyleColor();
			ImGui::PopFont();
		}
		Rule();
	}

	return open;
}

void LunarisUI::EndPanel()
{
	ImGui::EndChild();
}

bool LunarisUI::BeginWindow(const char* name, std::string_view label, bool* p_open, ImGuiWindowFlags flags)
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 18, 16 });
	const bool open = ImGui::Begin(name, nullptr, flags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();

	if (!open)
		return false;

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	const ImVec2 min = window->Pos;
	const ImVec2 max = { window->Pos.x + window->Size.x, window->Pos.y + window->Size.y };
	window->DrawList->PushClipRect(min, max, false);
	DrawGlassPanel(window->DrawList, min, max, 18.0f, true, true);
	window->DrawList->PopClipRect();

	// header: micro label, display title, close button
	const char* titleEnd = ImGui::FindRenderedTextEnd(name);
	if (!label.empty())
		MicroLabel(label);

	ImGui::PushFont(OrCurrent(Fonts().m_DisplayLarge));
	ImGui::PushStyleColor(ImGuiCol_Text, Ink);
	ImGui::TextUnformatted(name, titleEnd);
	ImGui::PopStyleColor();
	ImGui::PopFont();

	if (p_open)
	{
		constexpr float btn = 26.0f;
		const ImVec2 backup = ImGui::GetCursorPos();
		ImGui::SetCursorScreenPos({ max.x - btn - 30, min.y + 16 });
		ImGui::PushID("##close");
		const ImVec2 p = ImGui::GetCursorScreenPos();
		if (ImGui::InvisibleButton("x", { btn, btn }))
			*p_open = false;
		const bool hovered = ImGui::IsItemHovered();
		ImDrawList* dl = window->DrawList;
		AddParallelogram(dl, p, { p.x + btn, p.y + btn }, 5.0f, U32(hovered ? Orange : Teal, hovered ? 0.30f : 0.12f));
		StrokeParallelogram(dl, p, { p.x + btn, p.y + btn }, 5.0f, U32(hovered ? Orange : LineHot));
		const ImVec2 c = { p.x + btn * 0.5f, p.y + btn * 0.5f };
		const ImU32 xc = U32(hovered ? OrangeHi : Orange);
		dl->AddLine({ c.x - 4, c.y - 4 }, { c.x + 4, c.y + 4 }, xc, 1.5f);
		dl->AddLine({ c.x + 4, c.y - 4 }, { c.x - 4, c.y + 4 }, xc, 1.5f);
		ImGui::PopID();
		ImGui::SetCursorPos(backup);
	}

	Rule();
	ImGui::Spacing();
	return true;
}

void LunarisUI::MicroLabel(std::string_view text, const ImVec4& color, bool bar)
{
	ImFont* font = OrCurrent(Fonts().m_Label);
	const std::string upper = Upper(text);
	const float spacing = std::round(FontPx(font) * 0.24f);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 textSize = CalcSpacedTextSize(font, upper, spacing);
	const float lineH = std::max(textSize.y, ImGui::GetFrameHeight());
	const float textY = pos.y + (lineH - textSize.y) * 0.5f;

	float x = pos.x;
	if (bar)
	{
		const float barH = 7.0f;
		const float barY = pos.y + (lineH - barH) * 0.5f;
		AddParallelogram(dl, { x, barY }, { x + 16, barY + barH }, 5.0f, U32(color));
		x += 16 + 10;
	}

	DrawSpacedText(dl, font, { x, textY }, U32(color), upper, spacing);
	ImGui::Dummy({ (x - pos.x) + textSize.x, lineH });
}

bool LunarisUI::SkewButton(const char* label, ButtonStyle style, ImVec2 size)
{
	ImFont* font = OrCurrent(Fonts().m_Display);
	const char* labelEnd = ImGui::FindRenderedTextEnd(label);
	const std::string text = Upper(std::string_view(label, labelEnd - label));
	const float spacing = std::round(FontPx(font) * 0.08f);
	const ImVec2 textSize = CalcSpacedTextSize(font, text, spacing);

	constexpr float skew = 12.0f;
	if (size.x <= 0)
		size.x = textSize.x + 28 * 2;
	if (size.y <= 0)
		size.y = std::max(ImGui::GetFrameHeight() + 8, textSize.y + 18);

	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const bool pressed = ImGui::InvisibleButton(label, size);
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();

	const float dy = held ? 1.0f : 0.0f;
	const ImVec2 min = { pos.x, pos.y + dy };
	const ImVec2 max = { pos.x + size.x, pos.y + size.y + dy };
	const ImVec2 pts[4] = { { min.x + skew, min.y }, { max.x, min.y }, { max.x - skew, max.y }, { min.x, max.y } };

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec4 textColor;
	switch (style)
	{
	case ButtonStyle::Primary:
	{
		const ImVec4 top = hovered ? RGBA(255, 222, 150) : OrangeHi;
		const ImVec4 bottom = hovered ? Orange : OrangeDeep;
		FillConvexGradient(dl, pts, 4, top, bottom, { min.x, min.y }, { min.x, max.y });
		if (hovered)
			dl->AddPolyline(pts, 4, U32(Orange, 0.5f), ImDrawFlags_Closed, 3.0f); // glow-ish
		textColor = RGBA(0x1c, 0x0c, 0x00);
		break;
	}
	case ButtonStyle::Secondary:
	{
		const float boost = hovered ? 0.17f : 0.0f;
		FillConvexGradient(dl, pts, 4, RGBA(63, 230, 216, 0.28f + boost), RGBA(15, 109, 115, 0.35f + boost),
			{ min.x, min.y }, { min.x, max.y });
		dl->AddPolyline(pts, 4, U32(LineHot), ImDrawFlags_Closed, 1.0f);
		textColor = hovered ? RGBA(255, 255, 255) : TealHi;
		break;
	}
	case ButtonStyle::Ghost:
	default:
		dl->AddConvexPolyFilled(pts, 4, U32(Teal, hovered ? 0.16f : 0.08f));
		dl->AddPolyline(pts, 4, U32(Line), ImDrawFlags_Closed, 1.0f);
		textColor = hovered ? TealHi : Muted;
		break;
	}

	const ImVec2 textPos = { min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f };
	DrawSpacedText(dl, font, textPos, U32(textColor), text, spacing);

	return pressed;
}

bool LunarisUI::ToggleChip(const char* label, bool* value, bool warnWhenOff)
{
	ImFont* font = OrCurrent(Fonts().m_Label);
	const char* labelEnd = ImGui::FindRenderedTextEnd(label);
	const std::string text = Upper(std::string_view(label, labelEnd - label));
	const float spacing = std::round(FontPx(font) * 0.18f);
	const ImVec2 textSize = CalcSpacedTextSize(font, text, spacing);

	constexpr float skew = 7.0f;
	constexpr float dotSpace = 18.0f;
	const ImVec2 size = { textSize.x + dotSpace + 22, std::max(textSize.y + 12, 24.0f) };

	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const bool pressed = ImGui::InvisibleButton(label, size);
	if (pressed)
		*value = !*value;
	const bool hovered = ImGui::IsItemHovered();
	const bool on = *value;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 max = { pos.x + size.x, pos.y + size.y };
	if (on)
	{
		AddParallelogram(dl, pos, max, skew, U32(Teal, hovered ? 0.28f : 0.18f));
		StrokeParallelogram(dl, pos, max, skew, U32(LineHot));
	}
	else if (warnWhenOff)
	{
		AddParallelogram(dl, pos, max, skew, U32(Orange, hovered ? 0.14f : 0.06f));
		StrokeParallelogram(dl, pos, max, skew, U32(Orange, 0.7f));
	}
	else
	{
		AddParallelogram(dl, pos, max, skew, U32(Teal, hovered ? 0.12f : 0.04f));
		StrokeParallelogram(dl, pos, max, skew, U32(Line));
	}

	const ImVec2 dot = { pos.x + skew + 9, pos.y + size.y * 0.5f };
	if (on)
	{
		AddDiamond(dl, dot, 6.5f, U32(Green, 0.25f), true); // glow
		AddDiamond(dl, dot, 4.0f, U32(Green), true);
	}
	else
	{
		AddDiamond(dl, dot, 4.0f, U32(warnWhenOff ? Orange : Dim), false);
	}

	DrawSpacedText(dl, font, { pos.x + skew + dotSpace, pos.y + (size.y - textSize.y) * 0.5f },
		U32(on ? TealHi : (warnWhenOff ? Orange : Muted)), text, spacing);

	return pressed;
}

void LunarisUI::SegmentCounter(std::string_view label, int value, int digits, const ImVec4& color)
{
	ImFont* labelFont = OrCurrent(Fonts().m_Label);
	ImFont* segFont = OrCurrent(Fonts().m_Segment);

	const std::string upper = Upper(label);
	const float labelSpacing = std::round(FontPx(labelFont) * 0.24f);
	const ImVec2 labelSize = CalcSpacedTextSize(labelFont, upper, labelSpacing);

	char number[16]{};
	char ghost[16]{};
	digits = std::clamp(digits, 1, 8);
	std::snprintf(number, sizeof(number), "%0*d", digits, std::max(0, value));
	const int shown = static_cast<int>(std::strlen(number));
	for (int i = 0; i < shown && i < 15; i++)
		ghost[i] = '8';

	const float segPx = FontPx(segFont);
	const ImVec2 numSize = segFont->CalcTextSizeA(segPx, FLT_MAX, 0, ghost);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const float h = std::max(numSize.y, ImGui::GetFrameHeight());

	DrawSpacedText(dl, labelFont, { pos.x, pos.y + (h - labelSize.y) * 0.5f }, U32(Muted), upper, labelSpacing);

	const ImVec2 numPos = { pos.x + labelSize.x + 8, pos.y + (h - numSize.y) * 0.5f };
	dl->AddText(segFont, segPx, numPos, U32(color, 0.08f), ghost);                    // unlit segments
	dl->AddText(segFont, segPx, { numPos.x, numPos.y + 1 }, U32(color, 0.25f), number); // soft glow
	dl->AddText(segFont, segPx, numPos, U32(color), number);

	ImGui::Dummy({ labelSize.x + 8 + numSize.x, h });
}

void LunarisUI::Rule(float width)
{
	if (width <= 0)
		width = ImGui::GetContentRegionAvail().x;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const float y = pos.y + 3;
	const float lead = std::min(40.0f, width * 0.25f);
	dl->AddLine({ pos.x, y }, { pos.x + lead, y }, U32(Orange), 2.0f);
	dl->AddLine({ pos.x + lead + 4, y }, { pos.x + width, y }, U32(Line), 1.0f);
	ImGui::Dummy({ width, 7 });
}
