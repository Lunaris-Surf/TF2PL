#include "Log.h"
#include "Tests.h"

// Catch2 v3 (vcpkg) is a compiled library with its own runner; it writes results to stdout,
// so run tests from the console build: tf2_bot_detector_cli --run-tests [catch2 args...]
#include <catch2/catch_session.hpp>

#include <vector>

int tf2_bot_detector::RunTests(int argc, const char** argv)
{
	Log("Running tests...");

	Catch::Session session;
	const int result = session.run(argc, argv);

	if (result == 0)
		Log("Tests passed");
	else
		LogError("Tests failed ({} failing test case(s), see console output)", result);

	return result;
}

int tf2_bot_detector::RunTests()
{
	const char* argv[] = { "tf2_bot_detector" };
	return RunTests(1, argv);
}
