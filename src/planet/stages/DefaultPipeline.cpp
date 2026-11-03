#include "planet/PlanetLoadSteps.h"
#include "planet/stages/PlanetStages.h"

namespace Infinitesia
{
    // The data products of the default stages, by what they hold:
    //   tectonics  plates, crust, boundary context, surface rock
    //   terrain    elevation, slope, ocean
    //   geology    heat flow, sediment, hotspots, provinces, epochs
    //   climate    temperature, precipitation, seasons, continentality
    //   hydrology  rivers, lakes, drainage, the water balance
    //   soils      fertility and depth
    //   biomes     biome classification
    PlanetPipeline MakeDefaultPlanetPipeline()
    {
        PlanetPipeline pipeline;
        pipeline.Add({ "tectonics", 1, {}, { "tectonics" },
                       LoadStep::kTectonics, "Simulating tectonic plates", RunTectonics });
        pipeline.Add({ "terrain", 1, { "tectonics" }, { "terrain" },
                       LoadStep::kTerrain, "Shaping terrain", RunTerrain });
        pipeline.Add({ "geology", 1, { "terrain" }, { "geology" },
                       LoadStep::kGeology, "Mapping geological provinces", RunGeology });
        // Climate precedes hydrology so river generation can use precipitation.
        pipeline.Add({ "climate", 1, { "terrain" }, { "climate" },
                       LoadStep::kClimateContinentality, "Simulating climate", RunClimate });
        pipeline.Add({ "hydrology", 1, { "terrain", "climate" }, { "hydrology" },
                       LoadStep::kHydrology, "Carving rivers and lakes", RunHydrology });
        pipeline.Add({ "soils", 1, { "tectonics", "terrain", "climate", "hydrology" }, { "soils" },
                       LoadStep::kSoils, "Forming soils", RunSoils });
        pipeline.Add({ "biomes", 1, { "tectonics", "terrain", "climate", "hydrology", "soils" }, { "biomes" },
                       LoadStep::kBiomes, "Classifying biomes", RunBiomes });
        return pipeline;
    }
}
