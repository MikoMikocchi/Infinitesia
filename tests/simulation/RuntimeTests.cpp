#include <doctest/doctest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/Simulation.h"
#include "simulation/SimulationFixtures.h"
#include "simulation/runtime/SimulationRuntime.h"

using namespace InfinitesiaTests;

namespace
{
// One frame as long as one fixed tick, so exactly one tick runs per call.
void RunTicks(Simulation &simulation, int ticks)
{
	for (int i = 0; i < ticks; ++i)
	{
		simulation.AdvanceFrame(simulation.GetFixedDeltaTime());
	}
}

std::vector<std::string> RunScenario(int ticks)
{
	Simulation simulation(20.0f);
	SimulationRuntime runtime(simulation, GameCalendar(6, 60, 5));
	std::vector<std::string> log;
	AddRecording(runtime.Registry(), Describe("post", SystemPhase::PostUpdate), &log);
	AddRecording(runtime.Registry(), Describe("slow", SystemPhase::Update, Cadence::EveryTicks(7)), &log);
	AddRecording(runtime.Registry(), Describe("day", SystemPhase::PreUpdate, Cadence::EveryGameDays(1)), &log);
	AddRecording(runtime.Registry(), Describe("every"), &log);
	RunTicks(simulation, ticks);
	return log;
}
} // namespace

TEST_CASE("the runtime runs systems on the fixed ticks of the simulation")
{
	Simulation simulation(20.0f);
	SimulationRuntime runtime(simulation);
	RecordingSystem &system = AddRecording(runtime.Registry(), Describe("s"));

	// A long frame yields several ticks, each run with its own number.
	const std::uint64_t ticks = simulation.AdvanceFrame(0.26f);

	REQUIRE(ticks == 5);
	REQUIRE(system.contexts.size() == 5);
	for (std::uint64_t i = 0; i < 5; ++i)
	{
		CHECK(system.contexts[i].tick == i);
	}
	CHECK(simulation.GetTickCount() == 5);
	CHECK(runtime.Executor().CompletedTicks() == simulation.GetTickCount());
	CHECK(system.contexts[0].fixedDeltaTime == simulation.GetFixedDeltaTime());
}

TEST_CASE("the runtime adds no clock: game time follows the tick count")
{
	Simulation simulation(20.0f);
	SimulationRuntime runtime(simulation, GameCalendar(60, 86400, 30));
	RecordingSystem &system = AddRecording(runtime.Registry(), Describe("s"));
	RunTicks(simulation, 3);

	CHECK(system.contexts.back().time.seconds == 180);
	CHECK(runtime.Calendar().SecondsAfterTicks(simulation.GetTickCount()) == 180);
}

TEST_CASE("a frame with less than a tick runs nothing")
{
	Simulation simulation(20.0f);
	SimulationRuntime runtime(simulation);
	RecordingSystem &system = AddRecording(runtime.Registry(), Describe("s"));

	simulation.AdvanceFrame(0.01f);
	CHECK(system.contexts.empty());
}

TEST_CASE("the same setup gives the same run")
{
	const std::vector<std::string> first = RunScenario(60);
	const std::vector<std::string> second = RunScenario(60);
	CHECK_FALSE(first.empty());
	CHECK(first == second);
}

TEST_CASE("the runtime detaches from the simulation when destroyed")
{
	Simulation simulation(20.0f);
	{
		SimulationRuntime runtime(simulation);
		AddRecording(runtime.Registry(), Describe("s"));
		RunTicks(simulation, 1);
	}
	CHECK_NOTHROW(RunTicks(simulation, 2));
	CHECK(simulation.GetTickCount() == 3);
}

TEST_CASE("a failing system does not stop the simulation")
{
	Simulation simulation(20.0f);
	SimulationRuntime runtime(simulation);
	RecordingSystem &bad = AddRecording(runtime.Registry(), Describe("bad"));
	bad.onUpdate = [](const SystemContext &) { throw std::runtime_error("boom"); };

	CHECK_NOTHROW(RunTicks(simulation, 3));
	CHECK(simulation.GetTickCount() == 3);
	CHECK(runtime.Executor().Failures().size() == 1);
}
