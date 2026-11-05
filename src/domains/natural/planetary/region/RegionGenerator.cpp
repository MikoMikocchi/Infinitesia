#include "domains/natural/planetary/region/RegionGenerator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "domains/natural/planetary/internal/SurfaceModel.h"

namespace Infinitesia
{
RegionGenerator::RegionGenerator(const PlanetMap &map, std::shared_ptr<const WorldMap> world)
    : m_radiusMeters(map.GetPlanet().GetRadius()), m_terrain(map, std::move(world))
{
}

RegionData RegionGenerator::Generate(const LocalGrid &grid, RegionId id) const
{
	if (!LocalGrid::IsValid(id))
	{
		throw std::out_of_range("Region is too far from the grid anchor.");
	}

	RegionData data;
	data.id = id;
	data.center = grid.Center(id);
	data.cellSizeMeters = LocalGrid::kCellSizeMeters;

	const std::size_t count = static_cast<std::size_t>(RegionData::kPoints) * RegionData::kPoints;
	data.elevationMeters.resize(count);
	data.biome.resize(count);

	// The lattice resolves features down to two cells (Nyquist).
	const double minWavelength = data.cellSizeMeters * 2.0;

	float minimum = 0.0f;
	float maximum = 0.0f;
	for (int y = 0; y < RegionData::kPoints; ++y)
	{
		for (int x = 0; x < RegionData::kPoints; ++x)
		{
			const GeographicCoordinates position = PlanetaryCoordinates::ToGeographic(grid.PointDirection(id, x, y));
			const PlanetFieldSample planet = m_terrain.SampleAt(position);
			const float elevation = static_cast<float>(m_terrain.ElevationMeters(position, planet, minWavelength));

			const std::size_t index = RegionData::Index(x, y);
			data.elevationMeters[index] = elevation;
			data.biome[index] = GenerationDetail::ClassifyBiome(planet.surface, elevation);

			if (index == 0)
			{
				minimum = maximum = elevation;
			}
			minimum = std::min(minimum, elevation);
			maximum = std::max(maximum, elevation);
		}
	}
	data.minElevationMeters = minimum;
	data.maxElevationMeters = maximum;
	return data;
}

RegionPatch RegionGenerator::GeneratePatch(const LocalGrid &grid, RegionId center, int radiusRegions) const
{
	return GeneratePatch(grid, center, radiusRegions, LocalGrid::kCellSizeMeters);
}

RegionPatch RegionGenerator::GeneratePatch(const LocalGrid &grid, RegionId center, int radiusRegions,
                                           double cellSizeMeters) const
{
	if (!LocalGrid::IsValid(center))
	{
		throw std::out_of_range("Region is too far from the grid anchor.");
	}
	if (radiusRegions < 0 || radiusRegions > 8)
	{
		throw std::invalid_argument("Patch radius must be between 0 and 8 regions.");
	}
	if (!(cellSizeMeters > 0.0) || cellSizeMeters > LocalGrid::kRegionSizeMeters)
	{
		throw std::invalid_argument("Patch cell size is outside the region tile.");
	}

	const int cellsPerRegion = std::max(1, static_cast<int>(std::lround(LocalGrid::kRegionSizeMeters / cellSizeMeters)));
	RegionPatch patch;
	patch.center = center;
	patch.radiusRegions = radiusRegions;
	patch.points = (2 * radiusRegions + 1) * cellsPerRegion + 1;
	patch.cellSizeMeters = LocalGrid::kRegionSizeMeters / static_cast<double>(cellsPerRegion);
	patch.grid = grid;
	patch.originEastMeters = (center.i - 0.5 - radiusRegions) * LocalGrid::kRegionSizeMeters;
	patch.originNorthMeters = (center.j - 0.5 - radiusRegions) * LocalGrid::kRegionSizeMeters;

	const std::size_t count = static_cast<std::size_t>(patch.points) * patch.points;
	patch.elevationMeters.resize(count);
	patch.biome.resize(count);
	patch.surface.resize(count);

	const double minWavelength = patch.cellSizeMeters * 2.0;

	for (int y = 0; y < patch.points; ++y)
	{
		for (int x = 0; x < patch.points; ++x)
		{
			const GeographicCoordinates position = PlanetaryCoordinates::ToGeographic(patch.Direction(x, y));
			const PlanetFieldSample planet = m_terrain.SampleAt(position);
			const std::size_t index = patch.Index(x, y);
			const float elevation = static_cast<float>(m_terrain.ElevationMeters(position, planet, minWavelength));
			patch.elevationMeters[index] = elevation;
			patch.surface[index] = planet.surface;
			patch.biome[index] = GenerationDetail::ClassifyBiome(planet.surface, elevation);
		}
	}
	return patch;
}

RegionCache::RegionCache(const RegionGenerator &generator, LocalGrid grid, std::size_t capacity)
    : m_generator(generator), m_grid(grid), m_capacity(std::max<std::size_t>(1, capacity))
{
}

std::shared_ptr<const RegionData> RegionCache::Get(RegionId id)
{
	const auto found = m_entries.find(id);
	if (found != m_entries.end())
	{
		m_recent.splice(m_recent.begin(), m_recent, found->second.order);
		return found->second.data;
	}

	auto data = std::make_shared<const RegionData>(m_generator.Generate(m_grid, id));
	m_recent.push_front(id);
	m_entries.emplace(id, Entry{data, m_recent.begin()});

	while (m_entries.size() > m_capacity)
	{
		m_entries.erase(m_recent.back());
		m_recent.pop_back();
	}
	return data;
}
} // namespace Infinitesia
