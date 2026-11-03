#pragma once

#include "planet/PlanetRegion.h"
#include "raylib.h"

#include <cstddef>

// Colours of the planet viewer: how a region, a biome and a selection look.
namespace Infinitesia
{
    inline constexpr Color kSelectedCellColor = { 255, 202, 87, 255 };

    Color LerpColor(Color first, Color second, float amount);
    Color AverageColors(const Color* colors, std::size_t count);
    float SmootherStepF(float value);
    // The colour of a region from its biome, relief and water.
    Color GetRegionColor(const PlanetRegion& region);
    Color ShadeColor(Color color, float brightness);
    const char* BiomeName(Biome biome);
}
