#pragma once

#include "planet/PlanetRegion.h"

// What grows on the ground: soils and the biome classification.
namespace Infinitesia::GenerationDetail
{
    double SoilFertility(
        RockType rock,
        double temperatureCelsius,
        double precipitationMm,
        double planetAgeFraction);
    double SoilDepth(
        RockType rock,
        double precipitationMm,
        double slope,
        double planetAgeFraction);
    Biome ClassifyBiome(const PlanetRegion& region);
}
