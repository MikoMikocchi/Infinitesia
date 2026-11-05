#pragma once

#include "domains/natural/planetary/region/FluidOrigin.h"
#include "matter/MatterId.h"

namespace Infinitesia
{
// The fluid over one 1 m column, in metres above sea level. The material is
// universal: water, brine, oil, magma and anything else that is a fluid in the
// catalog. The block grid says where the ground is; this says what lies on it
// and how deep. `origin`/`matter` are only meaningful when `HasFluid()`.
struct FluidColumn
{
	MatterId matter = kAirMatter;
	FluidOrigin origin = FluidOrigin::None;
	// Top of the solid ground in this column.
	double groundMeters = 0.0;
	// Fluid surface; equal to `groundMeters` when the column is dry.
	double surfaceMeters = 0.0;
	// surfaceMeters - groundMeters, never negative.
	double depthMeters = 0.0;

	bool HasFluid() const noexcept
	{
		return origin != FluidOrigin::None;
	}
};
} // namespace Infinitesia
