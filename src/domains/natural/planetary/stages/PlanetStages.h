#pragma once

#include "domains/natural/planetary/pipeline/PlanetPipeline.h"

#include <vector>

namespace Infinitesia
{
    // The stages of the default planet, one function each (the default pipeline in
    // DefaultPipeline.cpp declares what each needs and provides). A stage reads and writes
    // the PlanetRegion fields of PlanetData::grid.
    void RunTectonics(PlanetBuild& build);
    void RunTerrain(PlanetBuild& build);
    void RunGeology(PlanetBuild& build);
    void RunProvinces(PlanetBuild& build);
    // The climate is three stages: continentality, then temperature, then precipitation.
    void RunClimateContinentality(PlanetBuild& build);
    void RunClimateTemperature(PlanetBuild& build);
    void RunClimatePrecipitation(PlanetBuild& build);
    void RunHydrology(PlanetBuild& build);
    void RunSoils(PlanetBuild& build);
    void RunBiomes(PlanetBuild& build);

    // Layer written by the geology stage: distance to the nearest oceanic crust, km per cell.
    inline constexpr const char* kDistanceToOceanLayer = "geology.distance_to_ocean";
}
