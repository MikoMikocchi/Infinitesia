#include "terrain_snapshot/SnapshotWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>

#include "domains/natural/planetary/StarSystemGenerator.h"

namespace SnapshotTool
{
using namespace Infinitesia;

bool MakeWindow(const Options &options, Window &window)
{
	window.extent = options.extentMeters;
	window.step = options.stepMeters;
	if (window.step <= 0.0)
	{
		window.step = std::max(1.0, std::ceil(window.extent / 256.0));
	}
	window.columns = std::max(1, static_cast<int>(std::lround(window.extent / window.step)));
	if (static_cast<long long>(window.columns) * window.columns > (1LL << 21))
	{
		std::fprintf(stderr,
		             "%d x %d columns is too many; use a larger --step or a "
		             "smaller --extent.\n",
		             window.columns, window.columns);
		return false;
	}
	return true;
}

namespace
{
PlanetMap BuildMap(const Options &options)
{
	const StarSystem system = StarSystemGenerator::Generate(GenerationScenario{.seed = options.seed});
	PlanetParameters parameters = system.planet;
	if (options.planetBands > 0)
	{
		parameters.latitudeBands = options.planetBands;
		parameters.longitudeSegments = options.planetBands * 2;
	}
	return PlanetMap(parameters, ClimateInputsFor(system));
}

LocalGrid BuildGrid(const Options &options, const RegionGenerator &generator)
{
	return generator.MakeGrid(PlanetaryCoordinates::ToCartesian({options.latitude, options.longitude}));
}
} // namespace

SnapshotWorld::SnapshotWorld(const Options &options)
    : m_map(BuildMap(options)), m_generator(m_map),
      m_matter(std::make_shared<const MatterCatalog>(
          MatterCatalog::LoadFromFile(std::filesystem::path(options.assets) / "matter" / "matter.xml"))),
      m_catalog(std::make_shared<const GeologyCatalog>(
          GeologyCatalog::Load(m_matter, std::filesystem::path(options.assets) / "geology" / "province_columns.json"))),
      m_registry(std::make_shared<const BlockRegistry>(
          BlockRegistry::Build(m_matter, std::filesystem::path(options.assets) / "blocks" / "shapes.json"))),
      m_geology(m_catalog, m_map),
      m_terrain(m_generator.GetTerrain(), BuildGrid(options, m_generator), options.originEast, options.originNorth)
{
}
} // namespace SnapshotTool
