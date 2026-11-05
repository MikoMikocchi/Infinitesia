#include "domains/natural/planetary/PlanetLoadSteps.h"
#include "domains/natural/planetary/stages/PlanetStages.h"

namespace Infinitesia
{
// The data products of the default stages, by what they hold:
//   tectonics  plates, crust, boundary context, surface rock
//   terrain    elevation, slope, ocean
//   geology    heat flow, sediment, hotspots, epochs, distance to ocean (layer)
//   provinces  province table and per-cell province ids
//   climate    temperature, precipitation, seasons, continentality
//   hydrology  rivers, lakes, drainage, the water balance
//   soils      fertility and depth
//   biomes     biome classification
PlanetPipeline MakeDefaultPlanetPipeline()
{
	PlanetPipeline pipeline;
	pipeline.Add({"tectonics", 1, {}, {"tectonics"}, LoadStep::kTectonics, "Simulating tectonic plates", RunTectonics});
	pipeline.Add({"terrain", 1, {"tectonics"}, {"terrain"}, LoadStep::kTerrain, "Shaping terrain", RunTerrain});
	pipeline.Add({"geology", 1, {"terrain"}, {"geology"}, LoadStep::kGeology, "Mapping geology", RunGeology});
	// Shares the geology load step: it has no bar of its own.
	pipeline.Add({"provinces", 1, {"geology"}, {"provinces"}, nullptr, "Mapping geological provinces", RunProvinces});
	// Climate precedes hydrology so river generation can use precipitation.
	pipeline.Add({"climate.continentality",
	              1,
	              {"terrain"},
	              {"climate.continentality"},
	              LoadStep::kClimateContinentality,
	              "Simulating climate",
	              RunClimateContinentality});
	pipeline.Add({"climate.temperature",
	              1,
	              {"terrain", "climate.continentality"},
	              {"climate.temperature"},
	              LoadStep::kClimateTemperature,
	              "Simulating climate: temperature",
	              RunClimateTemperature});
	pipeline.Add({"climate.precipitation",
	              1,
	              {"terrain", "climate.continentality", "climate.temperature"},
	              {"climate"},
	              LoadStep::kClimatePrecipitation,
	              "Simulating climate: precipitation",
	              RunClimatePrecipitation});
	pipeline.Add({"hydrology",
	              1,
	              {"terrain", "climate"},
	              {"hydrology"},
	              LoadStep::kHydrology,
	              "Carving rivers and lakes",
	              RunHydrology});
	pipeline.Add({"soils",
	              1,
	              {"tectonics", "terrain", "climate", "hydrology"},
	              {"soils"},
	              LoadStep::kSoils,
	              "Forming soils",
	              RunSoils});
	pipeline.Add({"biomes",
	              1,
	              {"tectonics", "terrain", "climate", "hydrology", "soils"},
	              {"biomes"},
	              LoadStep::kBiomes,
	              "Classifying biomes",
	              RunBiomes});
	return pipeline;
}
} // namespace Infinitesia
