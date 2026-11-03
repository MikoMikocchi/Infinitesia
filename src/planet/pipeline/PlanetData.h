#pragma once

#include "planet/Planet.h"
#include "planet/PlanetRegion.h"
#include "planet/geology/PlanetEpochs.h"
#include "planet/geology/Province.h"
#include "planet/pipeline/PlanetLayers.h"

#include <vector>

namespace Infinitesia
{
    // Everything a planet is made of, as the stages fill it in and PlanetMap serves it.
    struct PlanetData
    {
        Planet planet;
        ClimateInputs climate;
        std::vector<PlanetRegion> grid; // row-major [lat * lonSegments + lon]
        std::vector<Province> provinces;
        PlanetEpochTable epochs;
        PlanetWaterBalance waterBalance;
        PlanetLayers layers;
    };
}
