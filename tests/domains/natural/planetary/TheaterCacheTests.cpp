#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>

#include "core/PlanetId.h"
#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/RegionGenerator.h"
#include "domains/natural/planetary/region/SettlementTerrain.h"
#include "domains/natural/planetary/world/TheaterCache.h"
#include "domains/natural/planetary/world/WorldGenerator.h"

#define Require(condition, message)                                                                                    \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(condition))                                                                                              \
		{                                                                                                              \
			FAIL_CHECK(message);                                                                                       \
		}                                                                                                              \
	} while (0)

namespace
{
using namespace Infinitesia;

// A coarse planet keeps these tests fast in unoptimized builds.
const PlanetMap &SharedMap()
{
	static const PlanetMap map = [] {
		PlanetParameters parameters;
		parameters.latitudeBands = 90;
		parameters.longitudeSegments = 180;
		return PlanetMap(parameters);
	}();
	return map;
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
} // namespace

TEST_CASE("TheaterKeyIncludesThePlanet")
{
	const TheaterKey home{kHomePlanet, PlanetCellId{12, 34}};
	const TheaterKey other{kHomePlanet + 1, PlanetCellId{12, 34}};

	CHECK(home == TheaterKey{kHomePlanet, PlanetCellId{12, 34}});
	CHECK(home != other);
	CHECK(home.cell == other.cell);
	CHECK(home.planet != other.planet);

	std::unordered_set<TheaterKey> keys{home, other, home};
	CHECK(keys.size() == 2);
}

TEST_CASE("TheaterCacheResolvesAKeyToItsCellAnchor")
{
	TheaterCache cache(SharedMap());
	const TheaterKey key{kHomePlanet, PlanetCellId{41, 90}};
	const GeographicCoordinates anchor = cache.Anchor(key);

	CHECK(cache.KeyAt(anchor) == key);
	CHECK(cache.KeyAt(PlanetaryCoordinates::ToCartesian(anchor)) == key);
	CHECK(anchor.latitudeDegrees == doctest::Approx(SharedMap().GetPlanet().GetRegionCenter(41, 90).latitudeDegrees));
	CHECK(anchor.longitudeDegrees ==
	      doctest::Approx(SharedMap().GetPlanet().GetRegionCenter(41, 90).longitudeDegrees));
}

TEST_CASE("TheaterCacheGeneratesAsynchronouslyAndHitsTheSameWorldOnRepeat")
{
	TheaterCache cache(SharedMap(), kHomePlanet, 2);
	const TheaterKey key{kHomePlanet, PlanetCellId{44, 72}};

	// The first request starts the worker; it may already be done on a fast
	// machine, so only the eventual result is required here.
	std::shared_ptr<const WorldMap> first = cache.GetOrCreate(key);
	if (first == nullptr)
	{
		CHECK(cache.IsLoading(key));
		first = cache.WaitFor(key);
	}
	REQUIRE(first != nullptr);
	CHECK(cache.IsReady(key));

	// Re-entering the area must return the cached instance, not a new world.
	std::shared_ptr<const WorldMap> second = cache.GetOrCreate(key);
	REQUIRE(second != nullptr);
	CHECK(second == first);
	CHECK(cache.Size() == 1);
}

TEST_CASE("TheaterCacheIsDeterministicForAKey")
{
	TheaterCache first(SharedMap(), kHomePlanet, 1);
	TheaterCache second(SharedMap(), kHomePlanet, 1);
	const TheaterKey key{kHomePlanet, PlanetCellId{44, 72}};

	const std::shared_ptr<const WorldMap> a = first.WaitFor(key);
	const std::shared_ptr<const WorldMap> b = second.WaitFor(key);
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);
	REQUIRE(a->TileCount() == b->TileCount());
	REQUIRE(a->Rivers().size() == b->Rivers().size());

	for (int j = -kRegionTilesPerWorldHalfSide; j < kRegionTilesPerWorldHalfSide; ++j)
	{
		for (int i = -kRegionTilesPerWorldHalfSide; i < kRegionTilesPerWorldHalfSide; ++i)
		{
			Require(SameTile(a->At(i, j), b->At(i, j)), "The same theater key produced two different worlds.");
		}
	}
}

TEST_CASE("TheaterCacheEvictsTheLeastRecentlyUsedTheater")
{
	TheaterCache cache(SharedMap(), kHomePlanet, 2);
	const TheaterKey a{kHomePlanet, PlanetCellId{10, 10}};
	const TheaterKey b{kHomePlanet, PlanetCellId{20, 40}};
	const TheaterKey c{kHomePlanet, PlanetCellId{30, 60}};

	const std::shared_ptr<const WorldMap> aWorld = cache.WaitFor(a);
	const std::shared_ptr<const WorldMap> bWorld = cache.WaitFor(b);
	REQUIRE(aWorld != nullptr);
	REQUIRE(bWorld != nullptr);
	CHECK(cache.Size() == 2);

	// Touch a, then add c: b is now the least recently used and must go.
	CHECK(cache.GetOrCreate(a) == aWorld);
	const std::shared_ptr<const WorldMap> cWorld = cache.WaitFor(c);
	REQUIRE(cWorld != nullptr);
	CHECK(cache.Size() == 2);
	CHECK(cache.IsReady(a));
	CHECK(cache.IsReady(c));
	CHECK_FALSE(cache.IsReady(b));
	CHECK(cache.Peek(a) == aWorld);

	// Asking for the evicted key generates it again on a worker.
	CHECK(cache.GetOrCreate(b) == nullptr);
	CHECK(cache.IsLoading(b));
	CHECK(cache.WaitFor(b) != nullptr);
}

TEST_CASE("TerrainFallsBackToThePlanetWithoutATheater")
{
	RegionGenerator generator(SharedMap());
	const GeographicCoordinates position{20.0, 40.0};
	const CartesianPoint3D center = PlanetaryCoordinates::ToCartesian(position);
	REQUIRE(generator.GetTerrain().World() == nullptr);

	const PlanetFieldSample fallback = generator.GetTerrain().SampleAt(position);
	const PlanetFieldSample planet = generator.GetTerrain().Planet().SampleAt(position);
	CHECK(fallback.elevationMeters == doctest::Approx(planet.elevationMeters));
	CHECK(generator.GetTerrain().WaterAt(position, 16.0).origin == FluidOrigin::None);

	const SettlementTerrain settlement(generator.GetTerrain(), generator.MakeGrid(center), 0.0, 0.0);
	CHECK(settlement.World() == nullptr);
	CHECK(settlement.Column(0, 0).heightBlocks == 0);
}

TEST_CASE("TheaterCacheKeepsPlanetIdsApart")
{
	const TheaterKey home{kHomePlanet, PlanetCellId{5, 7}};
	const TheaterKey neighbour{kHomePlanet + 1, PlanetCellId{5, 7}};

	TheaterCache homeCache(SharedMap(), kHomePlanet, 1);
	TheaterCache neighbourCache(SharedMap(), kHomePlanet + 1, 1);

	CHECK(homeCache.KeyAt(homeCache.Anchor(home)) == home);
	CHECK(neighbourCache.KeyAt(neighbourCache.Anchor(neighbour)) == neighbour);
	CHECK(homeCache.HomePlanet() == kHomePlanet);
	CHECK(neighbourCache.HomePlanet() == kHomePlanet + 1);

	// A key that names another planet must not be able to reuse this planet's
	// map.
	CHECK_THROWS_AS(homeCache.GetOrCreate(neighbour), std::invalid_argument);
}
