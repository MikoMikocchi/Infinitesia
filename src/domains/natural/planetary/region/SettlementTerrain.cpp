#include "domains/natural/planetary/region/SettlementTerrain.h"

#include <cmath>

namespace Infinitesia
{
namespace
{
// Within this height of the waterline the ground is beach.
constexpr double kBeachMeters = 3.0;
} // namespace

SettlementTerrain::SettlementTerrain(const RegionTerrain &terrain, const LocalGrid &grid, double originEastMeters,
                                     double originNorthMeters)
    : m_terrain(terrain), m_grid(grid), m_originEastMeters(originEastMeters), m_originNorthMeters(originNorthMeters)
{
	const CartesianPoint3D origin = m_grid.Direction(m_originEastMeters, m_originNorthMeters);
	m_originElevationMeters = static_cast<int>(std::floor(m_terrain.ElevationMeters(origin, kMinWavelengthMeters)));
}

SurfaceKind SettlementTerrain::ClassifySurface(Biome biome, double elevationMeters) noexcept
{
	if (elevationMeters < kBeachMeters)
	{
		return SurfaceKind::Sand; // beach and sea floor
	}

	switch (biome)
	{
	case Biome::HotDesert:
	case Biome::ColdDesert:
	case Biome::SaltFlat:
		return SurfaceKind::Sand;
	case Biome::IceCap:
	case Biome::Glacier:
	case Biome::AlpineRock:
	case Biome::VolcanicTerrain:
		return SurfaceKind::Stone;
	case Biome::Tundra:
	case Biome::DeepOcean:
	case Biome::ShallowSea:
	case Biome::CoralReef:
	case Biome::PolarSea:
		return SurfaceKind::Dirt;
	default:
		return SurfaceKind::Grass;
	}
}

SurfaceColumn SettlementTerrain::Column(int eastBlocks, int northBlocks) const noexcept
{
	const CartesianPoint3D direction =
	    m_grid.Direction(m_originEastMeters + eastBlocks, m_originNorthMeters + northBlocks);
	const PlanetFieldSample planet = m_terrain.SamplePlanet(direction);
	const double elevation = m_terrain.ElevationMeters(direction, planet, kMinWavelengthMeters);
	const PlanetRegion &cell =
	    m_terrain.GetPlanetMap().GetRegion(planet.nearestLatitudeIndex, planet.nearestLongitudeIndex);
	const Biome biome = cell.biome;

	SurfaceColumn column;
	column.heightBlocks = static_cast<int>(std::floor(elevation)) - m_originElevationMeters;
	column.kind = ClassifySurface(biome, elevation);

	// The top of the topmost ground block. The block at level L has its bottom at
	// the origin elevation + (L - anchor), so the ground of this column ends one
	// metre above its surface block.
	const double groundTopMeters = static_cast<double>(m_originElevationMeters + column.heightBlocks + 1);

	if (elevation < 0.0 && groundTopMeters < 0.0)
	{
		column.fluid = FluidOrigin::Sea;
		column.fluidLevelMeters = 0.0;
	}
	else if (cell.isLake)
	{
		const double lakeLevel = cell.lakeWaterLevelKm * 1000.0;
		if (groundTopMeters < lakeLevel)
		{
			column.fluid = FluidOrigin::Lake;
			column.fluidLevelMeters = lakeLevel;
		}
	}
	column.province = cell.provinceId;
	column.pileTopMeters = static_cast<int>(std::floor(planet.elevationMeters));
	constexpr double kLapseRateCelsiusPerMeter = 0.0065;
	column.temperatureCelsius =
	    cell.temperatureCelsius - kLapseRateCelsiusPerMeter * (elevation - planet.elevationMeters);
	column.precipitationMm = cell.precipitationMm;
	column.drySeasonMonths = cell.drySeasonMonths;
	column.soilFertility = cell.soilFertility;
	column.soilDepth = cell.soilDepthNorm;
	column.elevationMeters = elevation;
	column.direction = direction;
	return column;
}
} // namespace Infinitesia
