#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <set>
#include <stdexcept>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/world/WorldGenerator.h"
#include "domains/natural/planetary/world/WorldHydrology.h"
#include "support/Require.h"

namespace
{
using namespace Infinitesia;

PlanetParameters CoarsePlanetParameters()
{
	PlanetParameters parameters;
	parameters.latitudeBands = 90;
	parameters.longitudeSegments = 180;
	parameters.seed = 42;
	return parameters;
}

// A land point with a river and a coastline nearby makes a useful world anchor:
// the 197 km window then contains land, sea, coast and at least one river.
GeographicCoordinates FindWorldAnchor(const PlanetMap &planet)
{
	GeographicCoordinates best{};
	double bestScore = -1.0;
	for (int latitude = 0; latitude < planet.GetLatitudeBandCount(); ++latitude)
	{
		for (int longitude = 0; longitude < planet.GetLongitudeSegmentCount(); ++longitude)
		{
			const PlanetRegion &region = planet.GetRegion(latitude, longitude);
			if (region.isOcean)
			{
				continue;
			}

			int oceanNeighbours = 0;
			if (latitude > 0 && planet.GetRegion(latitude - 1, longitude).isOcean)
			{
				++oceanNeighbours;
			}
			if (latitude + 1 < planet.GetLatitudeBandCount() && planet.GetRegion(latitude + 1, longitude).isOcean)
			{
				++oceanNeighbours;
			}
			if (planet.GetRegion(latitude, (longitude + 1) % planet.GetLongitudeSegmentCount()).isOcean)
			{
				++oceanNeighbours;
			}
			if (planet
			        .GetRegion(latitude,
			                   (longitude - 1 + planet.GetLongitudeSegmentCount()) % planet.GetLongitudeSegmentCount())
			        .isOcean)
			{
				++oceanNeighbours;
			}

			const double score = (region.hasRiver ? 4.0 : 0.0) + 2.0 * region.drainageAccumulation +
			                     0.5 * static_cast<double>(oceanNeighbours) + region.precipitationMm / 1000.0;
			if (score > bestScore)
			{
				bestScore = score;
				best = {region.latitudeDegrees, region.longitudeDegrees};
			}
		}
	}
	return best;
}

bool SameTile(const RegionTile &first, const RegionTile &second)
{
	return first.elevationMeters == second.elevationMeters && first.temperatureCelsius == second.temperatureCelsius &&
	       first.precipitationMm == second.precipitationMm && first.drySeasonMonths == second.drySeasonMonths &&
	       first.soilFertility == second.soilFertility && first.riverDischarge == second.riverDischarge &&
	       first.biome == second.biome && first.province == second.province && first.flags == second.flags;
}
} // namespace

TEST_CASE("WorldGridMatchesTheDwarfFortressHierarchy")
{
	static_assert(kTilesPerLocalBlock == 48);
	static_assert(kLocalBlocksPerRegionTile == 16);
	static_assert(kRegionTilesPerWorldSide == 257);
	static_assert(kChunksPerLocalBlock == 3);
	static_assert(kChunksPerRegionTile == 48);
	static_assert(kBlockSizeMeters == 1.0);
	static_assert(kLocalBlockSizeMeters == 48.0);
	static_assert(kRegionTileSizeMeters == 768.0);
	static_assert(kWorldSizeMeters == 197376.0);
	static_assert(sizeof(RegionTile) <= 32);

	CHECK(kRegionTilesPerWorldHalfSide == 128);
	CHECK(kRegionTilesPerWorldSide * kRegionTilesPerWorldSide == 66049);
}

TEST_CASE("WorldMapCoversA257By257WindowOfRegionTiles")
{
	const PlanetMap planet(CoarsePlanetParameters());
	const WorldGenerator generator(planet);
	const GeographicCoordinates anchor = FindWorldAnchor(planet);
	const WorldMap world = generator.Generate(anchor);

	REQUIRE(world.TileCount() == 66049);
	CHECK(world.Side() == 257);
	CHECK(world.Grid().PlanetRadiusMeters() == planet.GetPlanet().GetRadius());

	CHECK(WorldMap::Contains({0, 0}));
	CHECK(WorldMap::Contains({-128, -128}));
	CHECK(WorldMap::Contains({128, 128}));
	CHECK_FALSE(WorldMap::Contains({129, 0}));
	CHECK_FALSE(WorldMap::Contains({0, -129}));
	CHECK_THROWS_AS(world.At({129, 0}), std::out_of_range);

	std::size_t oceanTiles = 0;
	std::size_t lakeTiles = 0;
	std::size_t landTiles = 0;
	std::size_t coastalTiles = 0;
	std::size_t riverTiles = 0;
	std::set<int> biomes;
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			const RegionTile &tile = world.At(i, j);
			Require(std::isfinite(tile.elevationMeters), "A world tile has no finite elevation.");
			Require(std::isfinite(tile.temperatureCelsius), "A world tile has no finite temperature.");
			Require(std::isfinite(tile.precipitationMm), "A world tile has no finite precipitation.");
			Require(tile.precipitationMm >= 0.0f, "A world tile has negative precipitation.");
			Require(tile.riverDischarge >= 0.0f && tile.riverDischarge <= 1.0f,
			        "A world tile has a river discharge outside 0..1.");
			Require(tile.province == kNoProvince || tile.province <= planet.GetProvinces().size(),
			        "A world tile names a province that the planet does not have.");
			Require(tile.IsOcean() == (tile.elevationMeters < 0.0f),
			        "The ocean flag does not follow the world-tile elevation.");
			Require(!tile.HasRiver() || (!tile.IsOcean() && tile.riverDischarge > 0.0f),
			        "A river tile is outside the land or has no discharge.");

			oceanTiles += tile.IsOcean() ? 1 : 0;
			lakeTiles += tile.IsLake() ? 1 : 0;
			landTiles += tile.IsLand() ? 1 : 0;
			coastalTiles += tile.IsCoast() ? 1 : 0;
			riverTiles += tile.HasRiver() ? 1 : 0;
			biomes.insert(static_cast<int>(tile.biome));
		}
	}

	CHECK(oceanTiles + lakeTiles + landTiles == 66049);
	CHECK(oceanTiles > 0);
	CHECK(landTiles > 0);
	CHECK(coastalTiles > 0);
	CHECK(riverTiles > 0);
	CHECK(lakeTiles > 0);
	CHECK(biomes.size() >= 2);
}

TEST_CASE("WorldMapIsDeterministic")
{
	const PlanetMap planet(CoarsePlanetParameters());
	const WorldGenerator generator(planet);
	const GeographicCoordinates anchor = FindWorldAnchor(planet);
	const WorldMap first = generator.Generate(anchor);
	const WorldMap second = generator.Generate(anchor);

	REQUIRE(first.TileCount() == second.TileCount());
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			Require(SameTile(first.At(i, j), second.At(i, j)),
			        "World-map generation is not deterministic for an identical anchor.");
		}
	}
}

TEST_CASE("WorldMapChangesWhenTheAnchorMoves")
{
	const PlanetMap planet(CoarsePlanetParameters());
	const WorldGenerator generator(planet);
	const GeographicCoordinates anchor = FindWorldAnchor(planet);
	const WorldMap first = generator.Generate(anchor);
	const WorldMap second =
	    generator.Generate(GeographicCoordinates{anchor.latitudeDegrees + 0.5, anchor.longitudeDegrees + 0.5});

	std::size_t differences = 0;
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			differences += SameTile(first.At(i, j), second.At(i, j)) ? 0 : 1;
		}
	}
	CHECK(differences > 0);
}

TEST_CASE("WorldMapFitsTheStartupBudget")
{
	const auto start = std::chrono::steady_clock::now();
	const PlanetMap planet(CoarsePlanetParameters());
	const WorldGenerator generator(planet);
	const WorldMap world = generator.Generate(FindWorldAnchor(planet));
	const auto elapsed = std::chrono::steady_clock::now() - start;
#ifdef NDEBUG
	constexpr auto budget = std::chrono::seconds(2);
#else
	constexpr auto budget = std::chrono::seconds(60);
#endif
	Require(elapsed < budget, "World-map generation exceeded its startup budget.");
	Require(world.TileCount() == 66049, "The world map is incomplete.");
}

TEST_CASE("WorldHydrologyFillsBasinsAndRoutesRiversDownhill")
{
	std::vector<RegionTile> basin(66049);
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			RegionTile &tile = basin[WorldMap::Index({i, j})];
			tile.elevationMeters = 10.0f + 0.1f * static_cast<float>(std::abs(i) + std::abs(j));
			tile.precipitationMm = 1000.0f;
			tile.flags = 0;
		}
	}
	ComputeWorldHydrology(basin);
	CHECK(basin[WorldMap::Index({0, 0})].IsLake());

	std::vector<RegionTile> slope(66049);
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			RegionTile &tile = slope[WorldMap::Index({i, j})];
			tile.elevationMeters = 100.0f - 0.1f * static_cast<float>(i);
			tile.precipitationMm = 1000.0f;
			tile.flags = 0;
		}
	}
	ComputeWorldHydrology(slope);

	std::size_t rivers = 0;
	for (const RegionTile &tile : slope)
	{
		rivers += tile.HasRiver() ? 1 : 0;
	}
	CHECK(rivers > 0);
	CHECK(slope[WorldMap::Index({kRegionTilesPerWorldHalfSide, 0})].riverDischarge >
	      slope[WorldMap::Index({-kRegionTilesPerWorldHalfSide, 0})].riverDischarge);
}
