#include "simulation/runtime/SimulationRuntime.h"

#include <cstdint>

namespace Infinitesia
{
    SimulationRuntime::SimulationRuntime(Simulation& simulation, GameCalendar calendar, SystemErrorPolicy errorPolicy)
        : m_simulation(simulation),
          m_executor(errorPolicy),
          m_scheduler(m_registry, m_executor, calendar, simulation.GetFixedDeltaTime())
    {
        m_simulation.SetTickListener([this](std::uint64_t tick) { m_scheduler.RunTick(tick); });
    }

    SimulationRuntime::~SimulationRuntime()
    {
        m_simulation.SetTickListener(nullptr);
    }
}
