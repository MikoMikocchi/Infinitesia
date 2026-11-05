#include "domains/natural/planetary/world/WorldGenerator.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "core/PlanetaryCoordinates.h"
#include "domains/natural/planetary/internal/SurfaceModel.h"

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
} // namespace

WorldGenerator::WorldGenerator(const PlanetMap &map) noexcept
    : m_radiusMeters(map.GetPlanet().GetRadius()), m_terrain(map)
{
}

WorldMap WorldGenerator::Generate(CartesianPoint3D anchorDirection) const
{
	LocalGrid grid(m_radiusMeters, anchorDirection);
	std::vector<RegionTile> tiles(static_cast<std::size_t>(kRegionTilesPerWorldSide) *
	                              static_cast<std::size_t>(kRegionTilesPerWorldSide));

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
			tile.biome = GenerationDetail::ClassifyBiome(sample.surface, elevationMeters);
			tile.province = sample.surface.province;

			SetFlag(tile.flags, RegionTileFlag::Ocean, tile.elevationMeters < 0.0f);
			SetFlag(tile.flags, RegionTileFlag::Lake, sample.surface.lakeFraction >= 0.5);
			SetFlag(tile.flags, RegionTileFlag::River, tile.riverDischarge >= kRiverDischargeThreshold);

			tiles[WorldMap::Index({i, j})] = tile;
		}
	}

	MarkCoasts(tiles);
	return WorldMap(std::move(grid), std::move(tiles));
}

WorldMap WorldGenerator::Generate(const GeographicCoordinates &anchor) const
{
	return Generate(PlanetaryCoordinates::ToCartesian(anchor));
}
} // namespace Infinitesia
