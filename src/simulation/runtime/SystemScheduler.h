#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "simulation/api/GameCalendar.h"
#include "simulation/runtime/SystemExecutor.h"
#include "simulation/runtime/SystemRegistry.h"

namespace Infinitesia {
// Decides which systems run on a tick and in what order: phase by phase, inside
// a phase by `order` then registration order, skipping the disabled and those
// not yet due. The registry and executor must outlive it.
class SystemScheduler {
 public:
  SystemScheduler(SystemRegistry& registry, SystemExecutor& executor,
                  GameCalendar calendar, float fixedDeltaTime);

  // Runs tick number `tick`, which must follow the previous one.
  void RunTick(std::uint64_t tick);

  // The systems of a phase in the order they run (enabled or not).
  std::vector<SystemId> ExecutionOrder(SystemPhase phase);

  const GameCalendar& Calendar() const { return m_calendar; }

 private:
  void RebuildPlanIfStale();

  SystemRegistry& m_registry;
  SystemExecutor& m_executor;
  GameCalendar m_calendar;
  float m_fixedDeltaTime;
  std::array<std::vector<RegisteredSystem*>, kSystemPhases.size()> m_plan;
  std::uint64_t m_planRevision = 0;
  bool m_planBuilt = false;
};
}  // namespace Infinitesia
