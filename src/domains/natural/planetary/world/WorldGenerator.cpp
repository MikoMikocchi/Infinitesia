#include "domains/natural/planetary/world/WorldGenerator.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

#include "core/PlanetaryCoordinates.h"
#include "domains/natural/planetary/internal/ClimateModel.h"
#include "domains/natural/planetary/internal/SurfaceModel.h"
#include "domains/natural/planetary/world/WorldClimate.h"
#include "domains/natural/planetary/world/WorldHydrology.h"

namespace Infinitesia
{
namespace
{
constexpr double kLapseRateCelsiusPerMeter = 0.0065;

void MarkCoasts(std::vector<RegionTile> &tiles)
{
	const RegionTileId deltas[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			RegionTile &tile = tiles[WorldMap::Index({i, j})];
			if (!tile.IsLand())
			{
				continue;
			}

			for (const RegionTileId delta : deltas)
			{
				const RegionTileId neighbour{i + delta.i, j + delta.j};
				if (!WorldMap::Contains(neighbour))
				{
					continue;
				}
				const RegionTile &other = tiles[WorldMap::Index(neighbour)];
				if (other.IsOcean() || other.IsLake())
				{
					SetFlag(tile.flags, RegionTileFlag::Coast);
					break;
				}
			}
		}
	}
}

// Snow days from the annual mean. The planet stage counts months below zero;
// on the world tile the same signal is expressed with the local temperature
// and precipitation instead of a monthly series.
double LocalSnowDays(double temperatureCelsius, double precipitationMm) noexcept
{
	if (temperatureCelsius >= 0.0)
	{
		return 0.0;
	}
	const double coldFraction = std::clamp(-temperatureCelsius / 10.0, 0.0, 1.0);
	const double snowFraction = std::clamp(precipitationMm / 800.0, 0.0, 1.0);
	return 365.0 * coldFraction * snowFraction;
}

void ReclassifyBiomes(std::vector<RegionTile> &tiles, std::vector<PlanetSurface> &surfaces)
{
	for (std::size_t index = 0; index < tiles.size(); ++index)
	{
		RegionTile &tile = tiles[index];
		PlanetSurface &surface = surfaces[index];

		surface.temperatureCelsius = tile.temperatureCelsius;
		surface.precipitationMm = tile.precipitationMm;
		surface.drySeasonMonths = tile.drySeasonMonths;
		surface.soilFertility = tile.soilFertility;

		const double potentialEvapotranspiration =
		    GenerationDetail::AnnualEvapotranspirationMm(tile.temperatureCelsius);
		surface.soilMoisture =
		    std::clamp(static_cast<double>(tile.precipitationMm) /
			               (static_cast<double>(tile.precipitationMm) + potentialEvapotranspiration + 1.0),
			           0.0, 1.0);
		surface.snowDays = LocalSnowDays(tile.temperatureCelsius, tile.precipitationMm);

		// The local drainage network replaces the coarse planet accumulation,
		// and a locally filled basin reads as a lake.
		surface.drainageAccumulation = tile.riverDischarge;
		surface.lakeFraction = tile.IsLake() ? 1.0 : 0.0;

		tile.biome = GenerationDetail::ClassifyBiome(surface, tile.elevationMeters);
	}
}
} // namespace

WorldGenerator::WorldGenerator(const PlanetMap &map) noexcept
    : m_radiusMeters(map.GetPlanet().GetRadius()), m_terrain(map)
{
}

WorldMap WorldGenerator::Generate(CartesianPoint3D anchorDirection) const
{
	LocalGrid grid(m_radiusMeters, anchorDirection);
	const std::size_t tileCount =
	    static_cast<std::size_t>(kRegionTilesPerWorldSide) * static_cast<std::size_t>(kRegionTilesPerWorldSide);
	std::vector<RegionTile> tiles(tileCount);
	std::vector<PlanetSurface> surfaces(tileCount);
	std::vector<GeographicCoordinates> positions(tileCount);

	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			const double eastMeters = static_cast<double>(i) * kRegionTileSizeMeters;
			const double northMeters = static_cast<double>(j) * kRegionTileSizeMeters;
			const CartesianPoint3D direction = grid.Direction(eastMeters, northMeters);
			const GeographicCoordinates position = PlanetaryCoordinates::ToGeographic(direction);

			const PlanetFieldSample sample = m_terrain.SampleAt(position);
			const double elevationMeters = m_terrain.ElevationMeters(position, sample, kWorldDetailWavelengthMeters);

			RegionTile tile;
			tile.elevationMeters = static_cast<float>(elevationMeters);
			tile.temperatureCelsius =
			    static_cast<float>(sample.surface.temperatureCelsius -
				                   kLapseRateCelsiusPerMeter * (elevationMeters - sample.elevationMeters));
			tile.precipitationMm = static_cast<float>(sample.surface.precipitationMm);
			tile.drySeasonMonths = static_cast<float>(sample.surface.drySeasonMonths);
			tile.soilFertility = static_cast<float>(sample.surface.soilFertility);
			tile.riverDischarge = static_cast<float>(std::clamp(sample.surface.drainageAccumulation, 0.0, 1.0));
			tile.province = sample.surface.province;

			SetFlag(tile.flags, RegionTileFlag::Ocean, tile.elevationMeters < 0.0f);
			SetFlag(tile.flags, RegionTileFlag::Lake, sample.surface.lakeFraction >= 0.5);

			const std::size_t index = WorldMap::Index({i, j});
			tiles[index] = tile;
			surfaces[index] = sample.surface;
			positions[index] = position;
		}
	}

	// Local climate at region-tile resolution: the planet fields supply the
	// large-scale boundary, while water bodies, continentality, prevailing
	// winds and terrain produce rain shadows and orographic rain inside the
	// 197 x 197 km world.
	const ClimateInputs &planetClimate = m_terrain.GetPlanetMap().GetClimateInputs();
	WorldClimateInputs climate;
	climate.rotationPeriodHours = planetClimate.rotationPeriodHours;

	// First hydrology gives the local lakes and rivers that climate sees.
	// Climate changes precipitation, so hydrology runs once more before biomes.
	ComputeWorldHydrology(tiles);
	ComputeWorldClimate(tiles, std::span<const GeographicCoordinates>(positions), climate);
	std::vector<RiverChannel> rivers = ComputeWorldHydrology(tiles);
	MarkCoasts(tiles);
	ReclassifyBiomes(tiles, surfaces);

	return WorldMap(std::move(grid), std::move(tiles), std::move(rivers));
}

WorldMap WorldGenerator::Generate(const GeographicCoordinates &anchor) const
{
	return Generate(PlanetaryCoordinates::ToCartesian(anchor));
}
} // namespace Infinitesia
