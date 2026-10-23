#include "Application.h"
#include "Config/Settings.h"
#include "GameData/MatchmakingQueue.h"
#include "GameData/UserMessageType.h"
#include "UI/MainWindow.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "Util/RegexUtils.h"
#include "Log.h"
#include "WorldState.h"

#include <tf2pl/algorithm/multi_compare.hpp>
#include <tf2pl/text/charconv_helper.hpp>
#include <tf2pl/text/fmtstr.hpp>
#include <fmt/format.h>
#include <fmt/std.h>
#include <fmt/ostream.h>
#include <fmt/chrono.h>
#include <fmt/xchar.h>
#include <tf2pl/text/string_insertion.hpp>
#include <Util/ScopeGuards.h>

#include <regex>
#include <sstream>
#include <stdexcept>

#include "TeamsSwitchedLine.h"

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

// TODO: investigate if this even prints nowadays
std::shared_ptr<IConsoleLine> TeamsSwitchedLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	if (args.m_Text == "Teams have been switched."sv)
		return std::make_unique<TeamsSwitchedLine>(args.m_Timestamp);

	return nullptr;
}

bool TeamsSwitchedLine::ShouldPrint() const
{
#ifdef _DEBUG
	return true;
#else
	return false;
#endif
}

void TeamsSwitchedLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt({ 0.98f, 0.73f, 0.01f, 1 }, "Teams have been switched.");
}
