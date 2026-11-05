#include <doctest/doctest.h>

#include <stdexcept>

#include "simulation/SimulationFixtures.h"

using namespace InfinitesiaTests;

TEST_CASE("registry finds a registered system by id")
{
	SystemRegistry registry;
	AddRecording(registry, Describe("system.a"));
	AddRecording(registry, Describe("system.b"));

	CHECK(registry.Size() == 2);
	CHECK(registry.Contains(SystemId("system.a")));
	CHECK_FALSE(registry.Contains(SystemId("system.c")));
	REQUIRE(registry.Find(SystemId("system.b")) != nullptr);
	CHECK(registry.Find(SystemId("system.b"))->sequence == 1);
	CHECK(registry.Find(SystemId("system.c")) == nullptr);
}

TEST_CASE("registry rejects a repeated id")
{
	SystemRegistry registry;
	AddRecording(registry, Describe("system.a"));
	CHECK_THROWS_AS(AddRecording(registry, Describe("system.a")), std::invalid_argument);
	CHECK(registry.Size() == 1);
}

TEST_CASE("registry rejects bad registrations")
{
	SystemRegistry registry;
	CHECK_THROWS_AS(AddRecording(registry, Describe("")), std::invalid_argument);
	CHECK_THROWS_AS(registry.Register(Describe("system.null"), nullptr), std::invalid_argument);
	CHECK_THROWS_AS(AddRecording(registry, Describe("system.zero", SystemPhase::Update, Cadence::EveryTicks(0))),
	                std::invalid_argument);
	CHECK(registry.Size() == 0);
}

TEST_CASE("registry enables and disables systems")
{
	SystemRegistry registry;
	SystemDescriptor off = Describe("system.off");
	off.enabled = false;
	AddRecording(registry, off);
	AddRecording(registry, Describe("system.on"));

	CHECK_FALSE(registry.IsEnabled(SystemId("system.off")));
	CHECK(registry.IsEnabled(SystemId("system.on")));
	registry.SetEnabled(SystemId("system.off"), true);
	CHECK(registry.IsEnabled(SystemId("system.off")));
	CHECK_THROWS_AS(registry.SetEnabled(SystemId("system.none"), true), std::out_of_range);
}
