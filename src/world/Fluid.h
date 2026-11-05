#pragma once

#include <cstdint>

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
	// Block level of the top solid block of the column, and of the top fluid
	// block (-1 when the column is dry). Fluid fills the blocks strictly between
	// groundBlock and topBlock; the top one may be only partly full.
	int groundBlock = -1;
	int topBlock = -1;
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

// One cell of the fluid field (docs/tasks/MVP_FLUID_FIELD.md): what fluid lies
// in it and how much. A block cell holds geometry only; this is the separate,
// changeable layer. One cell holds one fluid, so different fluids never mix.
//
// A full cell is 1000 mB (one cubic metre, the whole 1x1x1 cell). The top cell
// of a column may hold less, which lets the surface sit between block levels.
inline constexpr std::uint16_t kFullFluidCellMb = 1000;

struct FluidCell
{
	MatterId matter = kAirMatter;
	std::uint16_t amountMb = 0;
	FluidOrigin origin = FluidOrigin::None;

	bool HasFluid() const noexcept
	{
		return amountMb > 0;
	}

	friend bool operator==(const FluidCell &, const FluidCell &) = default;
};
} // namespace Infinitesia
