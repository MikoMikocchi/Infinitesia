#pragma once

#include <cstdint>

#include "core/Simulation.h"
#include "simulation/api/GameCalendar.h"
#include "simulation/runtime/SimulationRuntime.h"

namespace Infinitesia
{
// One clock and one system runtime for the whole campaign: the planet, every
// site and, later, the political layer all see the same tick. Sites register
// their local systems here and enable only the active one.
//
// The campaign must outlive every LocalGame that registers its systems here;
// LocalGame unregisters its systems in its destructor.
class Campaign
{
public:
	explicit Campaign(float ticksPerSecond = 20.0f, GameCalendar calendar = GameCalendar{});
	~Campaign();

	Campaign(const Campaign &) = delete;
	Campaign &operator=(const Campaign &) = delete;

	// Advances the one clock; every due system runs on each fixed tick.
	std::uint64_t AdvanceFrame(float frameDelta);

	Simulation &Clock() noexcept
	{
		return m_clock;
	}
	const Simulation &Clock() const noexcept
	{
		return m_clock;
	}
	SimulationRuntime &Runtime() noexcept
	{
		return m_runtime;
	}
	const SimulationRuntime &Runtime() const noexcept
	{
		return m_runtime;
	}
	const GameCalendar &Calendar() const noexcept
	{
		return m_runtime.Calendar();
	}

	// The last whole month reached by the planet clock system. Zero until the
	// first month boundary.
	std::uint64_t ElapsedMonths() const noexcept
	{
		return m_months;
	}

private:
	Simulation m_clock;
	// Written by the planet clock system; declared before the runtime so it is
	// still alive while the runtime is destroyed.
	std::uint64_t m_months = 0;
	SimulationRuntime m_runtime;
};
} // namespace Infinitesia
