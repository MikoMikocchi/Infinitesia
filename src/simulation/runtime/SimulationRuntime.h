#pragma once

#include "core/Simulation.h"
#include "simulation/runtime/SystemExecutor.h"
#include "simulation/runtime/SystemRegistry.h"
#include "simulation/runtime/SystemScheduler.h"

namespace Infinitesia
{
// The registry, executor and scheduler wired to one `Simulation`: every fixed
// tick it produces runs the due systems. It adds no clock; game time is derived
// from the tick number. The simulation must outlive it, and it cannot be moved
// (the simulation points to it).
class SimulationRuntime
{
public:
	explicit SimulationRuntime(Simulation &simulation, GameCalendar calendar = GameCalendar{},
	                           SystemErrorPolicy errorPolicy = SystemErrorPolicy::Disable);
	~SimulationRuntime();

	SimulationRuntime(const SimulationRuntime &) = delete;
	SimulationRuntime &operator=(const SimulationRuntime &) = delete;

	SystemRegistry &Registry()
	{
		return m_registry;
	}
	const SystemRegistry &Registry() const
	{
		return m_registry;
	}
	SystemExecutor &Executor()
	{
		return m_executor;
	}
	const SystemExecutor &Executor() const
	{
		return m_executor;
	}
	SystemScheduler &Scheduler()
	{
		return m_scheduler;
	}
	const GameCalendar &Calendar() const
	{
		return m_scheduler.Calendar();
	}

private:
	Simulation &m_simulation;
	SystemRegistry m_registry;
	SystemExecutor m_executor;
	SystemScheduler m_scheduler;
};
} // namespace Infinitesia
