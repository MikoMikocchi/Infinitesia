#pragma once

#include <cstdint>

namespace Infinitesia {
// Where the fluid of a column comes from. The fluid itself is a material from
// the catalog (water, brine, oil, magma, ...); this says why it is there, which
// the simulation uses for its rules (a sea is an endless source, a lake
// evaporates, a volcanic pocket is hot). It deliberately says nothing about
// the material, so the same column model serves every fluid.
enum class FluidOrigin : std::uint8_t {
  None,
  Sea,
  Lake,
  River,
  Groundwater,
  Subsurface,
  Volcanic,
};
}  // namespace Infinitesia
