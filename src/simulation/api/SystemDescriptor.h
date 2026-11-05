#pragma once

#include "simulation/api/Cadence.h"
#include "simulation/api/SystemId.h"
#include "simulation/api/SystemPhase.h"

namespace Infinitesia
{
// What the scheduler needs to know about a system.
struct SystemDescriptor
{
	SystemId id;
	SystemPhase phase = SystemPhase::Update;
	Cadence cadence;
	// Within a phase systems run by ascending `order`; equal orders keep
	// registration order.
	int order = 0;
	bool enabled = true;
};
} // namespace Infinitesia
