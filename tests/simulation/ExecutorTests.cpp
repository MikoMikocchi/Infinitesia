#include <doctest/doctest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "simulation/SimulationFixtures.h"
#include "simulation/runtime/SystemExecutor.h"
#include "simulation/runtime/SystemScheduler.h"

using namespace InfinitesiaTests;

namespace {
void FailOnTick(RecordingSystem& system, std::uint64_t tick) {
  system.onUpdate = [tick](const SystemContext& context) {
    if (context.tick == tick) {
      throw std::runtime_error("boom");
    }
  };
}
}  // namespace

TEST_CASE("a failing system is recorded and disabled by default") {
  SystemRegistry registry;
  SystemExecutor executor;
  SystemScheduler scheduler(registry, executor, GameCalendar{}, 0.05f);
  std::vector<std::string> log;
  RecordingSystem& bad = AddRecording(
      registry, Describe("bad", SystemPhase::Update, Cadence::EveryTick(), 0),
      &log);
  AddRecording(registry,
               Describe("good", SystemPhase::Update, Cadence::EveryTick(), 1),
               &log);
  FailOnTick(bad, 1);

  for (std::uint64_t tick = 0; tick < 4; ++tick) {
    scheduler.RunTick(tick);
  }

  REQUIRE(executor.Failures().size() == 1);
  CHECK(executor.Failures()[0].id.Key() == "bad");
  CHECK(executor.Failures()[0].tick == 1);
  CHECK(executor.Failures()[0].message == "boom");
  CHECK_FALSE(registry.IsEnabled(SystemId("bad")));
  // "bad" ran on ticks 0 and 1 only; "good" was never held up.
  CHECK(bad.contexts.size() == 2);
  CHECK(std::count(log.begin(), log.end(), "good") == 4);
  CHECK(registry.Find(SystemId("bad"))->runCount == 1);
}

TEST_CASE("the continue policy keeps a failing system running") {
  SystemRegistry registry;
  SystemExecutor executor(SystemErrorPolicy::Continue);
  SystemScheduler scheduler(registry, executor, GameCalendar{}, 0.05f);
  RecordingSystem& bad = AddRecording(registry, Describe("bad"));
  FailOnTick(bad, 0);

  scheduler.RunTick(0);
  scheduler.RunTick(1);

  CHECK(executor.Failures().size() == 1);
  CHECK(registry.IsEnabled(SystemId("bad")));
  CHECK(bad.contexts.size() == 2);
  // The failed run did not count, so the next one covers both ticks.
  CHECK(bad.contexts[1].elapsedTicks == 2);
}

TEST_CASE(
    "the rethrow policy lets the exception out and the tick can be retried") {
  SystemRegistry registry;
  SystemExecutor executor(SystemErrorPolicy::Rethrow);
  SystemScheduler scheduler(registry, executor, GameCalendar{}, 0.05f);
  RecordingSystem& bad = AddRecording(registry, Describe("bad"));
  FailOnTick(bad, 0);

  CHECK_THROWS_WITH_AS(scheduler.RunTick(0), "boom", std::runtime_error);
  CHECK(executor.CompletedTicks() == 0);
  CHECK(executor.Failures().size() == 1);

  bad.onUpdate = nullptr;
  CHECK_NOTHROW(scheduler.RunTick(0));
  CHECK(executor.CompletedTicks() == 1);
}

TEST_CASE("non-standard exceptions are contained too") {
  SystemRegistry registry;
  SystemExecutor executor;
  SystemScheduler scheduler(registry, executor, GameCalendar{}, 0.05f);
  RecordingSystem& bad = AddRecording(registry, Describe("bad"));
  bad.onUpdate = [](const SystemContext&) { throw 42; };

  CHECK_NOTHROW(scheduler.RunTick(0));
  REQUIRE(executor.Failures().size() == 1);
  CHECK(executor.Failures()[0].message == "unknown exception");
}

TEST_CASE("ticks must run in sequence") {
  SystemRegistry registry;
  SystemExecutor executor;
  SystemScheduler scheduler(registry, executor, GameCalendar{}, 0.05f);

  CHECK_THROWS_AS(scheduler.RunTick(1), std::logic_error);
  scheduler.RunTick(0);
  CHECK_THROWS_AS(scheduler.RunTick(0), std::logic_error);
  CHECK_NOTHROW(scheduler.RunTick(1));
}

TEST_CASE(
    "the executor refuses work outside a tick or against the phase order") {
  SystemRegistry registry;
  SystemExecutor executor;
  AddRecording(registry, Describe("s"));
  RegisteredSystem& entry = *registry.Find(SystemId("s"));

  SystemContext context;
  context.tick = 0;
  context.phase = SystemPhase::PostUpdate;
  CHECK_THROWS_AS(executor.Execute(entry, context), std::logic_error);
  CHECK_THROWS_AS(executor.EndTick(), std::logic_error);

  executor.BeginTick(0);
  CHECK_NOTHROW(executor.Execute(entry, context));
  context.phase = SystemPhase::Update;
  CHECK_THROWS_AS(executor.Execute(entry, context), std::logic_error);
  context.phase = SystemPhase::PostUpdate;
  context.tick = 1;
  CHECK_THROWS_AS(executor.Execute(entry, context), std::logic_error);
}
