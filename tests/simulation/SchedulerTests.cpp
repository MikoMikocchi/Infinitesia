#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "simulation/SimulationFixtures.h"
#include "simulation/runtime/SystemExecutor.h"
#include "simulation/runtime/SystemScheduler.h"

using namespace InfinitesiaTests;

namespace
{
struct Rig
{
	SystemRegistry registry;
	SystemExecutor executor;
	GameCalendar calendar;
	SystemScheduler scheduler;
	std::vector<std::string> log;

	explicit Rig(GameCalendar calendarToUse = GameCalendar{})
	    : calendar(calendarToUse), scheduler(registry, executor, calendar, 0.05f)
	{
	}

	void Run(std::uint64_t ticks)
	{
		for (std::uint64_t i = 0; i < ticks; ++i)
		{
			scheduler.RunTick(executor.CompletedTicks());
		}
	}
};

// Ticks (0-based) on which the system ran.
std::vector<std::uint64_t> RunTicks(const RecordingSystem &system)
{
	std::vector<std::uint64_t> ticks;
	for (const SystemContext &context : system.contexts)
	{
		ticks.push_back(context.tick);
	}
	return ticks;
}
} // namespace

TEST_CASE("systems run by phase, then order, then registration")
{
	Rig rig;
	AddRecording(rig.registry, Describe("post", SystemPhase::PostUpdate), &rig.log);
	AddRecording(rig.registry, Describe("update.late", SystemPhase::Update, Cadence::EveryTick(), 5), &rig.log);
	AddRecording(rig.registry, Describe("pre", SystemPhase::PreUpdate), &rig.log);
	AddRecording(rig.registry, Describe("update.first", SystemPhase::Update, Cadence::EveryTick(), -1), &rig.log);
	AddRecording(rig.registry, Describe("update.a"), &rig.log);
	AddRecording(rig.registry, Describe("update.b"), &rig.log);

	rig.Run(1);

	const std::vector<std::string> expected{"pre", "update.first", "update.a", "update.b", "update.late", "post"};
	CHECK(rig.log == expected);
}

TEST_CASE("execution order lists a phase")
{
	Rig rig;
	AddRecording(rig.registry, Describe("b", SystemPhase::Update, Cadence::EveryTick(), 2));
	AddRecording(rig.registry, Describe("a", SystemPhase::Update, Cadence::EveryTick(), 1));
	AddRecording(rig.registry, Describe("p", SystemPhase::PostUpdate));

	const std::vector<SystemId> update = rig.scheduler.ExecutionOrder(SystemPhase::Update);
	REQUIRE(update.size() == 2);
	CHECK(update[0].Key() == "a");
	CHECK(update[1].Key() == "b");
	CHECK(rig.scheduler.ExecutionOrder(SystemPhase::PreUpdate).empty());
}

TEST_CASE("a system registered later joins the plan")
{
	Rig rig;
	AddRecording(rig.registry, Describe("first"), &rig.log);
	rig.Run(1);
	AddRecording(rig.registry, Describe("second", SystemPhase::PreUpdate), &rig.log);
	rig.Run(1);

	const std::vector<std::string> expected{"first", "second", "first"};
	CHECK(rig.log == expected);
}

TEST_CASE("cadence in ticks")
{
	Rig rig;
	RecordingSystem &every = AddRecording(rig.registry, Describe("every"));
	RecordingSystem &third = AddRecording(rig.registry, Describe("third", SystemPhase::Update, Cadence::EveryTicks(3)));

	rig.Run(9);

	CHECK(every.contexts.size() == 9);
	CHECK(RunTicks(third) == std::vector<std::uint64_t>{2, 5, 8});
	CHECK(third.contexts[1].elapsedTicks == 3);
}

TEST_CASE("cadence in game seconds, days and months")
{
	// 1 tick = 6 s, 60 s per day, 5 days per month: a day is 10 ticks, a
	// month 50.
	Rig rig(GameCalendar(6, 60, 5));
	RecordingSystem &seconds =
	    AddRecording(rig.registry, Describe("s", SystemPhase::Update, Cadence::EveryGameSeconds(18)));
	RecordingSystem &subTick =
	    AddRecording(rig.registry, Describe("sub", SystemPhase::Update, Cadence::EveryGameSeconds(1)));
	RecordingSystem &days = AddRecording(rig.registry, Describe("d", SystemPhase::Update, Cadence::EveryGameDays(1)));
	RecordingSystem &months =
	    AddRecording(rig.registry, Describe("m", SystemPhase::Update, Cadence::EveryGameMonths(1)));

	rig.Run(100);

	CHECK(seconds.contexts.size() == 33);
	CHECK(seconds.contexts.front().tick == 2);
	CHECK(subTick.contexts.size() == 100);
	CHECK(days.contexts.size() == 10);
	CHECK(days.contexts.front().tick == 9);
	CHECK(days.contexts.front().time.day == 1);
	CHECK(days.contexts[1].elapsedGameSeconds == 60);
	CHECK(RunTicks(months) == std::vector<std::uint64_t>{49, 99});
	CHECK(months.contexts.back().time.month == 2);
}

TEST_CASE("the first run of a system sees time since the start")
{
	Rig rig(GameCalendar(6, 60, 5));
	RecordingSystem &days = AddRecording(rig.registry, Describe("d", SystemPhase::Update, Cadence::EveryGameDays(1)));
	rig.Run(10);

	REQUIRE(days.contexts.size() == 1);
	CHECK(days.contexts[0].elapsedTicks == 10);
	CHECK(days.contexts[0].elapsedGameSeconds == 60);
	CHECK(days.contexts[0].fixedDeltaTime == doctest::Approx(0.05f));
}

TEST_CASE("disabled systems are skipped until enabled again")
{
	Rig rig;
	RecordingSystem &system = AddRecording(rig.registry, Describe("s"));

	rig.Run(2);
	rig.registry.SetEnabled(SystemId("s"), false);
	rig.Run(3);
	rig.registry.SetEnabled(SystemId("s"), true);
	rig.Run(1);

	CHECK(RunTicks(system) == std::vector<std::uint64_t>{0, 1, 5});
	// Its elapsed time covers the stretch it was off.
	CHECK(system.contexts.back().elapsedTicks == 4);
}

TEST_CASE("a system disabled from the start never runs")
{
	Rig rig;
	SystemDescriptor descriptor = Describe("s");
	descriptor.enabled = false;
	RecordingSystem &system = AddRecording(rig.registry, descriptor);
	rig.Run(5);
	CHECK(system.contexts.empty());
}

TEST_CASE("calendar rejects zero lengths and splits time")
{
	CHECK_THROWS_AS(GameCalendar(0, 60, 5), std::invalid_argument);
	CHECK_THROWS_AS(GameCalendar(6, 0, 5), std::invalid_argument);
	CHECK_THROWS_AS(GameCalendar(6, 60, 0), std::invalid_argument);

	const GameCalendar calendar(6, 60, 5);
	const GameTime time = calendar.At(60 * 7 + 13);
	CHECK(time.day == 7);
	CHECK(time.month == 1);
	CHECK(time.dayOfMonth == 2);
	CHECK(time.secondOfDay == 13);
}
