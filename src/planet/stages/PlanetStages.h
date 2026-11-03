#pragma once

#include "planet/pipeline/PlanetPipeline.h"

#include <vector>

namespace Infinitesia
{
    // The stages of the default planet, one function each (the default pipeline in
    // DefaultPipeline.cpp declares what each needs and provides). A stage reads and writes
    // the PlanetRegion fields of PlanetData::grid.
    void RunTectonics(PlanetBuild& build);
    void RunTerrain(PlanetBuild& build);
    void RunGeology(PlanetBuild& build);
    void RunClimate(PlanetBuild& build);
    void RunHydrology(PlanetBuild& build);
    void RunSoils(PlanetBuild& build);
    void RunBiomes(PlanetBuild& build);

    // Part of the geology stage: groups the cells into provinces.
    void GenerateProvinces(PlanetBuild& build, const std::vector<double>& distanceToOceanKm);
}
