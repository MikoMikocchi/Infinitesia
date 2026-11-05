#pragma once

#include "domains/natural/planetary/PlanetRegion.h"

// What grows on the ground: soils and the biome classification.
namespace Infinitesia::GenerationDetail
{
double SoilFertility(RockType rock, double temperatureCelsius, double precipitationMm, double planetAgeFraction);
double SoilDepth(RockType rock, double precipitationMm, double slope, double planetAgeFraction);
Biome ClassifyBiome(const PlanetRegion &region);
// Classifies the interpolated fields at a local elevation, so the biome follows
// the zero-height contour and the continuous climate instead of a cell border.
Biome ClassifyBiome(const PlanetSurface &surface, double elevationMeters);
} // namespace Infinitesia::GenerationDetail
