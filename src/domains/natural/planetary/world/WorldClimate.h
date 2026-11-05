#pragma once

#include <span>
#include <vector>

#include "core/PlanetaryCoordinates.h"
#include "domains/natural/planetary/world/WorldMap.h"

namespace Infinitesia
{
// Climate parameters that the world-scale model needs from the planet. The
// planet fields already carry the global energy balance and circulation; the
// local model adds the effects of the 257 x 257 terrain and water bodies.
struct WorldClimateInputs
{
	double rotationPeriodHours = 24.0;
};

// Direction the prevailing wind blows towards, in the local east/north frame.
// The vector is unit length. Rotation changes the width of the tropical,
// mid-latitude and polar belts, as in the planet climate stage.
struct PrevailingWind
{
	double east = 0.0;
	double north = 0.0;
};

PrevailingWind WorldPrevailingWind(double latitudeDegrees, double rotationPeriodHours) noexcept;

// Recomputes temperature and precipitation on the world grid from the
// planetary boundary fields already stored in `tiles`:
//
//   temperature  lapse rate is applied by WorldGenerator; here large water
//                bodies pull coastal air toward the water surface temperature
//                and continental interiors get a colder-winter / hotter-summer
//                annual offset.
//   precipitation prevailing winds carry ocean moisture inland; windward slopes
//                lift it (orographic rain), while high ground upwind leaves a
//                rain shadow on the lee side.
//
// `positions` holds the tile centre for the same order as `tiles`
// (WorldMap::Index). The function is deterministic.
void ComputeWorldClimate(std::vector<RegionTile> &tiles, std::span<const GeographicCoordinates> positions,
                         const WorldClimateInputs &inputs);
} // namespace Infinitesia
