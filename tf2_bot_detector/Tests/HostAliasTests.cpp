#include "WebServer/HostAlias.h"
#include <catch2/catch_test_macros.hpp>

using namespace tf2_bot_detector::WebHostAlias;

TEST_CASE("Web UI hosts alias detects existing and conflicting mappings", "[TF2PL][webui]")
{
	REQUIRE(Inspect("") == State::Missing);
	REQUIRE(Inspect("# 127.0.0.1 local.tf2pl\n127.0.0.1 localhost\n") == State::Missing);
	REQUIRE(Inspect("127.0.0.1 local.tf2pl.example\n") == State::Missing);
	REQUIRE(Inspect("127.0.0.1\tlocalhost LOCAL.TF2PL\r\n") == State::Ready);
	REQUIRE(Inspect("127.0.0.1 local.tf2pl. # TF2PL\n") == State::Ready);
	REQUIRE(Inspect("127.0.0.1 local.tf2pl\n127.0.0.1 local.tf2pl\n") == State::Ready);
	REQUIRE(Inspect("192.0.2.1 local.tf2pl\n") == State::Conflict);
	REQUIRE(Inspect("127.0.0.1 local.tf2pl\n::1 local.tf2pl\n") == State::Conflict);
	REQUIRE(Inspect("192.0.2.1 local.tf2pl\n127.0.0.1 local.tf2pl\n") == State::Conflict);
}

TEST_CASE("Web UI host validation allows only local names on its port", "[TF2PL][webui]")
{
	REQUIRE(IsAllowedHost("local.TF2PL:8765", 8765));
	REQUIRE(IsAllowedHost("127.0.0.1:8765", 8765));
	REQUIRE(IsAllowedHost("localhost:8765", 8765));
	REQUIRE(IsAllowedHost("local.tf2pl", 80));
	REQUIRE(IsAllowedHost("local.tf2pl:80", 80));
	REQUIRE_FALSE(IsAllowedHost("local.tf2pl", 8765));
	REQUIRE_FALSE(IsAllowedHost("local.tf2pl:9999", 8765));
	REQUIRE_FALSE(IsAllowedHost("local.tf2pl.evil.example:8765", 8765));
	REQUIRE_FALSE(IsAllowedHost("evil.example:8765", 8765));
	REQUIRE_FALSE(IsAllowedHost("local.tf2pl:8765@evil.example", 8765));
}
