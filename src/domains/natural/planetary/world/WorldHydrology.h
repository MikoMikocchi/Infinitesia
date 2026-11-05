#pragma once

#include <vector>

#include "domains/natural/planetary/world/WorldMap.h"

namespace Infinitesia
{
// Effective catchment area (km^2, weighted by precipitation) above which a
// region tile is marked as a river.
inline constexpr float kRiverAreaKm2 = 25.0f;

// A depression deeper than this is a lake on the world map.
inline constexpr float kLakeDepthMeters = 1.0f;

// Runs a local drainage network over the 257 x 257 world map: fills closed
// depressions to their spill level, routes each land tile to its lowest
// neighbour, accumulates precipitation-weighted catchment area and marks rivers
// and lakes. The coarse planet hydrology is only a boundary condition; this is
// the world's own water.
std::vector<RiverChannel> ComputeWorldHydrology(std::vector<RegionTile> &tiles);
} // namespace Infinitesia
