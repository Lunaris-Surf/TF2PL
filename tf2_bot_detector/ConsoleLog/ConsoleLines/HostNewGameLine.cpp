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

#include "HostNewGameLine.h"

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

std::shared_ptr<IConsoleLine> HostNewGameLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	if (args.m_Text == "---- Host_NewGame ----"sv)
		return std::make_shared<HostNewGameLine>(args.m_Timestamp);

	return nullptr;
}

void HostNewGameLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("---- Host_NewGame ----"sv);
}
