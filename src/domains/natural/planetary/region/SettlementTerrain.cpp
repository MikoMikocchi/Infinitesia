#include "domains/natural/planetary/region/SettlementTerrain.h"

#include <cmath>

#include "domains/natural/planetary/internal/SurfaceModel.h"

namespace Infinitesia
{
namespace
{
// Within this height of the waterline the ground is beach.
constexpr double kBeachMeters = 3.0;

// The flat frame is centred on the site, not on the grid anchor: a site can be
// dozens of regions from the anchor, and the equirectangular approximation is
// most accurate around its own centre.
LocalFrame FrameAtOrigin(const LocalGrid &grid, double originEastMeters, double originNorthMeters)
{
	return LocalFrame::At(PlanetaryCoordinates::ToGeographic(grid.Direction(originEastMeters, originNorthMeters)));
}
} // namespace

SettlementTerrain::SettlementTerrain(const RegionTerrain &terrain, const LocalGrid &grid, double originEastMeters,
                                     double originNorthMeters)
    : SettlementTerrain(terrain, FrameAtOrigin(grid, originEastMeters, originNorthMeters), originEastMeters,
	                    originNorthMeters)
{
}

SettlementTerrain::SettlementTerrain(const RegionTerrain &terrain, const LocalFrame &frame,
                                     double geologyOriginEastMeters, double geologyOriginNorthMeters)
    : m_terrain(terrain), m_frame(frame), m_originEastMeters(geologyOriginEastMeters),
      m_originNorthMeters(geologyOriginNorthMeters)
{
	m_originElevationMeters =
	    static_cast<int>(std::floor(m_terrain.ElevationMeters(m_frame.center, kMinWavelengthMeters)));
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
	const GeographicCoordinates position = m_frame.ToGeographic(eastBlocks, northBlocks);
	const PlanetFieldSample planet = m_terrain.SampleAt(position);
	const double elevation = m_terrain.ElevationMeters(position, planet, kMinWavelengthMeters);
	const Biome biome = GenerationDetail::ClassifyBiome(planet.surface, elevation);

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
	else if (planet.surface.lakeFraction > 0.0)
	{
		const double lakeLevel = planet.surface.lakeWaterLevelKm * 1000.0;
		if (groundTopMeters < lakeLevel)
		{
			column.fluid = FluidOrigin::Lake;
			column.fluidLevelMeters = lakeLevel;
		}
	}
	column.province = planet.surface.province;
	column.pileTopMeters = static_cast<int>(std::floor(planet.elevationMeters));
	constexpr double kLapseRateCelsiusPerMeter = 0.0065;
	column.temperatureCelsius =
	    planet.surface.temperatureCelsius - kLapseRateCelsiusPerMeter * (elevation - planet.elevationMeters);
	column.precipitationMm = planet.surface.precipitationMm;
	column.drySeasonMonths = planet.surface.drySeasonMonths;
	column.soilFertility = planet.surface.soilFertility;
	column.soilDepth = planet.surface.soilDepthNorm;
	column.elevationMeters = elevation;
	column.position = position;
	return column;
}
} // namespace Infinitesia
