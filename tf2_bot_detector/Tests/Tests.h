#pragma once

#ifdef TF2BD_ENABLE_TESTS
namespace tf2_bot_detector
{
	// argv[0] is treated as the program name; the rest are passed to Catch2 (e.g. "[PlayerRuleTests]", "-s").
	int RunTests(int argc, const char** argv);
	int RunTests();
}
#endif
