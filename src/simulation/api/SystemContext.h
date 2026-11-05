#pragma once

#include <cstdint>

#include "simulation/api/GameCalendar.h"
#include "simulation/api/SystemPhase.h"

namespace Infinitesia
{
// What a system is told about the run it is being given.
struct SystemContext
{
	std::uint64_t tick = 0; // 0-based number of the tick being run
	SystemPhase phase = SystemPhase::Update;
	float fixedDeltaTime = 0.0f; // real seconds of one tick
	GameTime time;               // game time at the end of this tick
	// Since the system last ran (since the start for its first run); a system on
	// a slow cadence scales its work by these.
	std::uint64_t elapsedTicks = 0;
	std::uint64_t elapsedGameSeconds = 0;
};
} // namespace Infinitesia
