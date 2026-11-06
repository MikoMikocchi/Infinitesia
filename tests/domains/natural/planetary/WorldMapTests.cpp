#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/world/WorldClimate.h"
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
	       first.waterLevelMeters == second.waterLevelMeters && first.waterDepthMeters == second.waterDepthMeters &&
	       first.riverFirst == second.riverFirst && first.riverCount == second.riverCount &&
	       first.flowEast == second.flowEast && first.flowNorth == second.flowNorth && first.biome == second.biome &&
	       first.province == second.province && first.flags == second.flags;
}

bool SameRiver(const RiverChannel &first, const RiverChannel &second)
{
	return first.fromX == second.fromX && first.fromY == second.fromY && first.toX == second.toX &&
	       first.toY == second.toY && first.widthMeters == second.widthMeters &&
	       first.depthMeters == second.depthMeters && first.bendX == second.bendX && first.bendY == second.bendY;
}

std::span<const RiverChannel> ChannelsOf(const RegionTile &tile, const std::vector<RiverChannel> &rivers)
{
	if (tile.riverCount == 0 || tile.riverFirst >= rivers.size())
	{
		return {};
	}
	const std::size_t count = std::min<std::size_t>(tile.riverCount, rivers.size() - tile.riverFirst);
	return {rivers.data() + tile.riverFirst, count};
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
	static_assert(sizeof(RegionTile) <= 64);

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
	REQUIRE(first.Rivers().size() == second.Rivers().size());
	for (std::size_t index = 0; index < first.Rivers().size(); ++index)
	{
		Require(SameRiver(first.Rivers()[index], second.Rivers()[index]),
		        "World-map river channels are not deterministic.");
	}
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

TEST_CASE("WorldHydrologyStoresLakeLevelsAndRiverChannels")
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
	const RegionTile &lake = basin[WorldMap::Index({0, 0})];
	REQUIRE(lake.IsLake());
	CHECK(lake.waterLevelMeters > lake.elevationMeters);
	CHECK(lake.waterDepthMeters > kLakeDepthMeters);
	CHECK(lake.riverCount == 0);
	CHECK(lake.HasWater());

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
	const std::vector<RiverChannel> rivers = ComputeWorldHydrology(slope);

	std::size_t channelCount = 0;
	for (const RegionTile &tile : slope)
	{
		if (!tile.HasRiver())
		{
			continue;
		}
		REQUIRE(tile.flowEast >= 0);
		REQUIRE(tile.flowNorth == 0);
		const std::span<const RiverChannel> channels = ChannelsOf(tile, rivers);
		if (!channels.empty())
		{
			CHECK(tile.waterLevelMeters < tile.elevationMeters);
		}
		for (const RiverChannel &channel : channels)
		{
			++channelCount;
			CHECK(channel.widthMeters >= 3.0f);
			CHECK(channel.widthMeters <= 40.0f);
			CHECK(channel.depthMeters >= 1.0f);
			CHECK_GE(channel.fromX, 0.0f);
			CHECK_LE(channel.fromX, 1.0f);
			CHECK_GE(channel.fromY, 0.0f);
			CHECK_LE(channel.fromY, 1.0f);
			CHECK_GE(channel.toX, 0.0f);
			CHECK_LE(channel.toX, 1.0f);
			CHECK_GE(channel.toY, 0.0f);
			CHECK_LE(channel.toY, 1.0f);
			REQUIRE(channel.HasBend());
			CHECK_GE(channel.bendX, 0.0f);
			CHECK_LE(channel.bendX, 1.0f);
			CHECK_GE(channel.bendY, 0.0f);
			CHECK_LE(channel.bendY, 1.0f);
		}
	}
	CHECK(channelCount > 0);

	// A river flowing east enters at the west border and leaves at the east
	// border of its tile; a single-inlet tile has one straight segment.
	const RegionTile &middle = slope[WorldMap::Index({0, 0})];
	REQUIRE(middle.HasRiver());
	const std::span<const RiverChannel> middleChannels = ChannelsOf(middle, rivers);
	REQUIRE(!middleChannels.empty());
	CHECK(middle.flowEast == 1);
}

TEST_CASE("WorldHydrologyBuildsAConfluenceWithSeveralTributaries")
{
	std::vector<RegionTile> tiles(66049);
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			RegionTile &tile = tiles[WorldMap::Index({i, j})];
			tile.elevationMeters = 100.0f + 0.1f * static_cast<float>(std::abs(i) + std::abs(j));
			if (j == 0 && i >= 0)
			{
				tile.elevationMeters = 100.0f - 0.5f * static_cast<float>(i);
			}
			tile.precipitationMm = 1000.0f;
			tile.flags = 0;
		}
	}

	const std::vector<RiverChannel> rivers = ComputeWorldHydrology(tiles);
	const RegionTile &confluence = tiles[WorldMap::Index({0, 0})];
	REQUIRE(confluence.HasRiver());
	REQUIRE(confluence.riverCount >= 3);
	CHECK(ChannelsOf(confluence, rivers).size() == confluence.riverCount);

	// The tributary segments are narrower than the combined outlet segment.
	const std::span<const RiverChannel> channels = ChannelsOf(confluence, rivers);
	float narrowest = channels.front().widthMeters;
	float widest = channels.front().widthMeters;
	for (const RiverChannel &channel : channels)
	{
		narrowest = std::min(narrowest, channel.widthMeters);
		widest = std::max(widest, channel.widthMeters);
	}
	CHECK(widest > narrowest);
}

TEST_CASE("WorldMapLocatesPointsInsideRegionTiles")
{
	const LocalGrid grid(6371000.0, PlanetaryCoordinates::ToCartesian({0.0, 0.0}));
	std::vector<RegionTile> tiles(66049);
	for (std::size_t index = 0; index < tiles.size(); ++index)
	{
		tiles[index].elevationMeters = static_cast<float>(index);
	}
	const WorldMap world(grid, std::move(tiles));

	const RegionTileLocation center = world.Locate(grid.Direction(0.0, 0.0));
	REQUIRE(center);
	CHECK(center.id == RegionTileId{0, 0});
	CHECK(center.x == doctest::Approx(0.5));
	CHECK(center.y == doctest::Approx(0.5));

	const RegionTileLocation east =
	    world.Locate(grid.Direction(0.25 * kRegionTileSizeMeters, -0.25 * kRegionTileSizeMeters));
	REQUIRE(east);
	CHECK(east.id == RegionTileId{0, 0});
	CHECK(east.x == doctest::Approx(0.75));
	CHECK(east.y == doctest::Approx(0.25));

	const RegionTileLocation next =
	    world.Locate(grid.Direction(0.75 * kRegionTileSizeMeters, 0.75 * kRegionTileSizeMeters));
	REQUIRE(next);
	CHECK(next.id == RegionTileId{1, 1});
	CHECK(next.x == doctest::Approx(0.25));
	CHECK(next.y == doctest::Approx(0.25));

	CHECK_FALSE(world.Locate(grid.Direction(200.0 * kRegionTileSizeMeters, 0.0)));
}

TEST_CASE("WorldMapBlendsTileBordersInsteadOfStepping")
{
	const LocalGrid grid(6371000.0, PlanetaryCoordinates::ToCartesian({0.0, 0.0}));
	std::vector<RegionTile> tiles(66049);
	tiles[WorldMap::Index({0, 0})].elevationMeters = 0.0f;
	tiles[WorldMap::Index({1, 0})].elevationMeters = 100.0f;
	const WorldMap world(grid, std::move(tiles));

	const auto west = world.SampleAt(grid.Direction(kRegionTileSizeMeters * 0.49, 0.0));
	const auto middle = world.SampleAt(grid.Direction(kRegionTileSizeMeters * 0.5, 0.0));
	const auto east = world.SampleAt(grid.Direction(kRegionTileSizeMeters * 0.51, 0.0));

	REQUIRE(west.has_value());
	REQUIRE(middle.has_value());
	REQUIRE(east.has_value());
	CHECK(middle->elevationMeters == doctest::Approx(50.0).epsilon(0.01));
	CHECK(east->elevationMeters > west->elevationMeters);
	CHECK(east->elevationMeters - west->elevationMeters < 5.0);
}

namespace
{
std::vector<GeographicCoordinates> PositionsAtLatitude(double latitudeDegrees)
{
	std::vector<GeographicCoordinates> positions(66049);
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			positions[WorldMap::Index({i, j})] = {latitudeDegrees, 0.0};
		}
	}
	return positions;
}
} // namespace

TEST_CASE("WorldClimateModeratesTheCoastAndCoolsContinentalInteriors")
{
	const std::vector<GeographicCoordinates> positions = PositionsAtLatitude(45.0);
	std::vector<RegionTile> tiles(66049);
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			RegionTile &tile = tiles[WorldMap::Index({i, j})];
			tile.precipitationMm = 1000.0f;
			if (i < -100)
			{
				SetFlag(tile.flags, RegionTileFlag::Ocean);
				tile.elevationMeters = -200.0f;
				tile.temperatureCelsius = 20.0f;
			}
			else
			{
				tile.elevationMeters = 0.0f;
				tile.temperatureCelsius = 0.0f;
			}
		}
	}

	ComputeWorldClimate(tiles, std::span<const GeographicCoordinates>(positions), WorldClimateInputs{});

	const RegionTile &coast = tiles[WorldMap::Index({-99, 0})];
	const RegionTile &interior = tiles[WorldMap::Index({0, 0})];
	CHECK(coast.temperatureCelsius > interior.temperatureCelsius + 2.0f);
}

TEST_CASE("WorldClimateRaisesOrographicRainAndLeavesARainShadow")
{
	const std::vector<GeographicCoordinates> positions = PositionsAtLatitude(45.0);
	std::vector<RegionTile> tiles(66049);
	for (int j = -kRegionTilesPerWorldHalfSide; j <= kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i <= kRegionTilesPerWorldHalfSide; ++i)
		{
			RegionTile &tile = tiles[WorldMap::Index({i, j})];
			tile.precipitationMm = 1000.0f;
			tile.temperatureCelsius = 10.0f;
			if (i < -100)
			{
				SetFlag(tile.flags, RegionTileFlag::Ocean);
				tile.elevationMeters = -100.0f;
			}
			else if (std::abs(i) <= 2)
			{
				tile.elevationMeters = 2500.0f;
			}
			else
			{
				tile.elevationMeters = 100.0f;
			}
		}
	}

	ComputeWorldClimate(tiles, std::span<const GeographicCoordinates>(positions), WorldClimateInputs{});

	// At 45 degrees the prevailing wind is westerly, so the west slope (-2)
	// is windward and the east slope (+2) is in the lee of the ridge.
	const float windward = tiles[WorldMap::Index({-2, 0})].precipitationMm;
	const float lee = tiles[WorldMap::Index({2, 0})].precipitationMm;
	CHECK(windward > 1000.0f);
	CHECK(lee < windward * 0.5f);
}
