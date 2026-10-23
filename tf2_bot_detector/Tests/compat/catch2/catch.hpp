#pragma once

// Catch2 v2 -> v3 compatibility: v3 removed the single <catch2/catch.hpp> header. A few app sources still embed
// tests with the old include; this makes them build against v3. New code should include the specific v3 headers
// (e.g. <catch2/catch_test_macros.hpp>) instead.
#include <catch2/catch_all.hpp>
