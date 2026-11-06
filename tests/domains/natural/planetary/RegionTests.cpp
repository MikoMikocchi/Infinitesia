#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <utility>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/LocalFrame.h"
#include "domains/natural/planetary/region/LocalGrid.h"
#include "domains/natural/planetary/region/ProvinceMix.h"
#include "domains/natural/planetary/region/RegionGenerator.h"
#include "domains/natural/planetary/region/SettlementTerrain.h"
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

constexpr double kRadius = 6371000.0;

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

double Distance(CartesianPoint3D a, CartesianPoint3D b)
{
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

CartesianPoint3D Place(double latitude, double longitude)
{
	return PlanetaryCoordinates::ToCartesian({latitude, longitude});
}

double EastwardDegrees(double from, double to)
{
	double delta = to - from;
	if (delta > 180.0)
		delta -= 360.0;
	if (delta < -180.0)
		delta += 360.0;
	return delta;
}

// Anchors spread over the planet: tropics, mid-latitudes, the longitude seam
// and both poles.
const GeographicCoordinates kAnchors[] = {{0.0, 0.0},    {35.0, 100.0}, {-50.0, 10.0}, {70.0, 359.99},
                                          {-9.8, 126.8}, {89.0, 40.0},  {-90.0, 0.0},  {90.0, 0.0}};
} // namespace

TEST_CASE("RegionsAreEqualSquaresOfOneRegionTile")
{
	static_assert(LocalGrid::kRegionSizeMeters == 768.0);
	static_assert(LocalGrid::kCellSizeMeters == 16.0);

	for (const GeographicCoordinates anchor : kAnchors)
	{
		const LocalGrid grid(kRadius, Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
		for (const RegionId id : {RegionId{0, 0}, RegionId{3, -2}, RegionId{-20, 14}, RegionId{40, 40}})
		{
			// Side lengths of the region measured along the surface.
			const double east =
			    Distance(grid.PointDirection(id, 0, 0), grid.PointDirection(id, LocalGrid::kCellsPerRegionEdge, 0)) *
			    kRadius;
			const double north =
			    Distance(grid.PointDirection(id, 0, 0), grid.PointDirection(id, 0, LocalGrid::kCellsPerRegionEdge)) *
			    kRadius;
			// Within ~160 km of the anchor the projection stretches by less than
			// 0.1%.
			Require(std::abs(east - LocalGrid::kRegionSizeMeters) < 1.0 &&
			            std::abs(north - LocalGrid::kRegionSizeMeters) < 1.0,
			        "Region is not a region tile square near the anchor.");
		}
	}
}

TEST_CASE("RegionsFormAChessboard")
{
	for (const GeographicCoordinates anchor : kAnchors)
	{
		const LocalGrid grid(kRadius, Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
		for (const RegionId id : {RegionId{0, 0}, RegionId{5, -7}, RegionId{-30, 22}})
		{
			const int edge = LocalGrid::kCellsPerRegionEdge;
			for (int k = 0; k <= edge; k += 16)
			{
				// Each side meets exactly one neighbour, point for point.
				Require(Distance(grid.PointDirection(id, edge, k), grid.PointDirection({id.i + 1, id.j}, 0, k)) < 1e-12,
				        "Eastern border does not meet the eastern neighbour.");
				Require(Distance(grid.PointDirection(id, 0, k), grid.PointDirection({id.i - 1, id.j}, edge, k)) < 1e-12,
				        "Western border does not meet the western neighbour.");
				Require(Distance(grid.PointDirection(id, k, edge), grid.PointDirection({id.i, id.j + 1}, k, 0)) < 1e-12,
				        "Northern border does not meet the northern neighbour.");
				Require(Distance(grid.PointDirection(id, k, 0), grid.PointDirection({id.i, id.j - 1}, k, edge)) < 1e-12,
				        "Southern border does not meet the southern neighbour.");
			}
		}
	}
}

TEST_CASE("RegionAtInvertsCenterDirectionAndProjectInvertsDirection")
{
	for (const GeographicCoordinates anchor : kAnchors)
	{
		const LocalGrid grid(kRadius, Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
		Require(grid.RegionAt(Place(anchor.latitudeDegrees, anchor.longitudeDegrees)) == RegionId{},
		        "The anchor is not in region (0, 0).");
		for (const RegionId id :
		     {RegionId{0, 0}, RegionId{1, 0}, RegionId{0, -1}, RegionId{-12, 9}, RegionId{100, 100}})
		{
			Require(grid.RegionAt(grid.CenterDirection(id)) == id, "RegionAt does not invert CenterDirection.");
		}
		for (const double east : {-150000.0, -4096.0, 0.0, 3000.5, 90000.0})
		{
			double e = 0.0;
			double n = 0.0;
			grid.Project(grid.Direction(east, -2.5 * east), e, n);
			Require(std::abs(e - east) < 1e-3 && std::abs(n + 2.5 * east) < 1e-3, "Project does not invert Direction.");
		}
	}
}

TEST_CASE("GridIsNorthUpAndEastRight")
{
	// Away from the poles the grid axes follow the compass at the anchor.
	for (const GeographicCoordinates anchor : kAnchors)
	{
		if (std::abs(anchor.latitudeDegrees) > 85.0)
		{
			continue;
		}
		const LocalGrid grid(kRadius, Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
		const GeographicCoordinates here = PlanetaryCoordinates::ToGeographic(grid.Direction(0.0, 0.0));
		const GeographicCoordinates north = PlanetaryCoordinates::ToGeographic(grid.Direction(0.0, 5000.0));
		const GeographicCoordinates east = PlanetaryCoordinates::ToGeographic(grid.Direction(5000.0, 0.0));
		Require(north.latitudeDegrees > here.latitudeDegrees, "Grid y axis does not run north.");
		Require(EastwardDegrees(here.longitudeDegrees, east.longitudeDegrees) > 0.0, "Grid x axis does not run east.");
		Require(std::abs(EastwardDegrees(here.longitudeDegrees, north.longitudeDegrees)) < 1e-6,
		        "Grid y axis is not along the meridian.");
	}
}

TEST_CASE("TheGridStretchesLittleFarFromTheAnchor")
{
	const LocalGrid grid(kRadius, Place(45.0, 100.0));
	// A region 1600 km out (the limit) is stretched by about 3%.
	const RegionId outermost{LocalGrid::kMaxRegionIndex, 0};
	const double side = Distance(grid.PointDirection(outermost, 0, 0),
	                             grid.PointDirection(outermost, LocalGrid::kCellsPerRegionEdge, 0)) *
	                    kRadius;
	Require(side < LocalGrid::kRegionSizeMeters && side > LocalGrid::kRegionSizeMeters * 0.9,
	        "Distance stretch at the grid limit is out of range.");
	Require(LocalGrid::IsValid(outermost) && !LocalGrid::IsValid({LocalGrid::kMaxRegionIndex + 1, 0}),
	        "Grid limit is not enforced by IsValid.");
}

TEST_CASE("LocalFrameIsFlatAndInverts")
{
	for (const GeographicCoordinates center :
	     {GeographicCoordinates{45.0, 100.0}, GeographicCoordinates{-9.8, 359.9}, GeographicCoordinates{89.0, 0.0}})
	{
		const LocalFrame frame = LocalFrame::At(center);
		for (const auto [east, north] : {std::pair{0.0, 0.0}, std::pair{1500.0, -2200.0}, std::pair{-4096.0, 4096.0}})
		{
			const GeographicCoordinates position = frame.ToGeographic(east, north);
			double backEast = 0.0;
			double backNorth = 0.0;
			frame.ToLocal(position, backEast, backNorth);
			Require(std::abs(backEast - east) < 1e-6 && std::abs(backNorth - north) < 1e-6,
			        "LocalFrame does not invert its own conversion.");
		}
		// A degree of longitude shrinks towards the poles; the frame follows.
		Require(frame.metersPerDegreeEast <= LocalFrame::kMetersPerDegree,
		        "LocalFrame has more metres per degree of longitude than at the equator.");
	}
}

TEST_CASE("RegionGenerationIsDeterministic")
{
	const RegionGenerator generator(SharedMap());
	const LocalGrid grid = generator.MakeGrid(Place(35.0, 80.0));
	const RegionData first = generator.Generate(grid, {2, -1});
	const RegionData second = generator.Generate(grid, {2, -1});
	Require(first.elevationMeters == second.elevationMeters, "Region elevation is not deterministic.");
	Require(first.biome == second.biome, "Region biomes are not deterministic.");
	Require(first.elevationMeters.size() == static_cast<std::size_t>(RegionData::kPoints) * RegionData::kPoints,
	        "Region lattice has the wrong size.");
}

TEST_CASE("WorldMapDrivesTheLocalTerrainInsideItsWindow")
{
	const PlanetMap &planet = SharedMap();
	const LocalGrid grid(kRadius, Place(35.0, 80.0));
	const RegionId region{2, -1};

	std::vector<RegionTile> tiles(static_cast<std::size_t>(kRegionTilesPerWorldSide) *
	                              static_cast<std::size_t>(kRegionTilesPerWorldSide));
	RegionTile &tile = tiles[WorldMap::Index({region.i, region.j})];
	tile.elevationMeters = 1234.0f;
	tile.temperatureCelsius = -10.0f;
	tile.precipitationMm = 300.0f;
	tile.drySeasonMonths = 0.0f;
	tile.soilFertility = 0.4f;
	tile.biome = Biome::Tundra;
	tile.flags = 0;

	const auto world = std::make_shared<const WorldMap>(grid, std::move(tiles));
	const RegionGenerator generator(planet, world);
	const GeographicCoordinates center = grid.Center(region);

	const PlanetFieldSample sample = generator.GetTerrain().SampleAt(center);
	Require(sample.elevationMeters == doctest::Approx(1234.0), "The world tile did not replace the base elevation.");
	Require(sample.surface.temperatureCelsius == doctest::Approx(-10.0),
	        "The world tile did not replace the temperature.");
	Require(sample.surface.precipitationMm == doctest::Approx(300.0),
	        "The world tile did not replace the precipitation.");

	const RegionData data = generator.Generate(grid, region);
	double mean = 0.0;
	for (const float elevation : data.elevationMeters)
	{
		mean += elevation;
	}
	mean /= static_cast<double>(data.elevationMeters.size());
	Require(std::abs(mean - 1234.0) < 2000.0, "The local region ignored the world tile's base elevation.");

	const SettlementTerrain settlement(generator.GetTerrain(), grid, region.i * LocalGrid::kRegionSizeMeters,
	                                   region.j * LocalGrid::kRegionSizeMeters);
	const SurfaceColumn column = settlement.Column(0, 0);
	Require(std::abs(column.temperatureCelsius + 10.0) < 5.0,
	        "SettlementTerrain did not use the world tile's temperature.");
	Require(std::abs(column.elevationMeters - 1234.0) < 2000.0,
	        "SettlementTerrain did not use the world tile's base elevation.");

	// Outside the 257 x 257 window the planet field is still the fallback.
	const RegionId outside{300, 0};
	const PlanetFieldSample fallback = generator.GetTerrain().SampleAt(grid.Center(outside));
	Require(fallback.surface.temperatureCelsius != doctest::Approx(-10.0),
	        "A region outside the world window borrowed the world tile.");
}

TEST_CASE("WorldRiversCarveChannelsAndCarryWater")
{
	const PlanetMap &planet = SharedMap();
	const CartesianPoint3D anchor = Place(35.0, 80.0);
	const LocalGrid grid(kRadius, anchor);

	std::vector<RegionTile> tiles(static_cast<std::size_t>(kRegionTilesPerWorldSide) *
	                              static_cast<std::size_t>(kRegionTilesPerWorldSide));
	for (RegionTile &tile : tiles)
	{
		tile.elevationMeters = 100.0f;
		tile.temperatureCelsius = 15.0f;
		tile.precipitationMm = 1000.0f;
		tile.flags = 0;
	}
	RegionTile &river = tiles[WorldMap::Index({0, 0})];
	SetFlag(river.flags, RegionTileFlag::River);
	river.riverFirst = 0;
	river.riverCount = 3;
	river.waterLevelMeters = 99.75f;
	river.waterDepthMeters = 2.0f;
	river.flowEast = 1;
	std::vector<RiverChannel> channels{
	    RiverChannel{0.0f, 0.5f, 0.5f, 0.5f, 20.0f, 2.0f},
	    RiverChannel{0.5f, 1.0f, 0.5f, 0.5f, 12.0f, 1.5f},
	    RiverChannel{0.5f, 0.5f, 1.0f, 0.5f, 20.0f, 2.0f},
	};

	const auto world = std::make_shared<const WorldMap>(grid, std::move(tiles), std::move(channels));
	const RegionGenerator generator(planet, world);
	const WorldTerrain &terrain = generator.GetTerrain();

	const GeographicCoordinates center = grid.Center({0, 0});
	const PlanetFieldSample sample = terrain.SampleAt(center);
	const double uncarved = terrain.Planet().ElevationMeters(center, sample, 2.0);
	const double carved = terrain.ElevationMeters(center, sample, 2.0);
	const LocalWater water = terrain.WaterAt(center, 2.0);

	REQUIRE(water.origin == FluidOrigin::River);
	CHECK(water.incisionMeters > 1.5);
	CHECK(carved < uncarved - 1.5);
	CHECK(water.surfaceMeters < uncarved);
	CHECK(water.flowEast == 1.0);
	CHECK(water.flowNorth == 0.0);

	// Away from the channel the ground is untouched and dry.
	const GeographicCoordinates bank = PlanetaryCoordinates::ToGeographic(grid.Direction(60.0, 60.0));
	const LocalWater bankWater = terrain.WaterAt(bank, 2.0);
	CHECK(bankWater.origin == FluidOrigin::None);
	CHECK(bankWater.incisionMeters == 0.0);
	CHECK(bankWater.bankRaiseMeters == 0.0);
	CHECK(terrain.SampleAt(bank).elevationMeters == doctest::Approx(100.0));

	// The water's edge is below the surface, but the dry bank immediately
	// outside it is raised to stand above the water. Without this lip the
	// fluid field simply levels the river across the surrounding land.
	const GeographicCoordinates west = PlanetaryCoordinates::ToGeographic(grid.Direction(-380.0, 0.0));
	const LocalWater westWater = terrain.WaterAt(west, 2.0);
	REQUIRE(westWater.origin == FluidOrigin::River);
	const GeographicCoordinates shore = PlanetaryCoordinates::ToGeographic(grid.Direction(-380.0, 9.5));
	const LocalWater shoreWater = terrain.WaterAt(shore, 2.0);
	CHECK(shoreWater.origin == FluidOrigin::River);
	CHECK(terrain.ElevationMeters(shore, 2.0) < westWater.surfaceMeters);
	const GeographicCoordinates lip = PlanetaryCoordinates::ToGeographic(grid.Direction(-380.0, 12.0));
	const LocalWater lipWater = terrain.WaterAt(lip, 2.0);
	CHECK(lipWater.origin == FluidOrigin::None);
	CHECK(lipWater.surfaceMeters == doctest::Approx(westWater.surfaceMeters));
	CHECK(terrain.ElevationMeters(lip, 2.0) > lipWater.surfaceMeters);

	// The local world hands the river to SettlementTerrain: the centre is in
	// the channel and wet, the bank is dry and higher.
	const SettlementTerrain settlement(terrain, grid, 0.0, 0.0);
	const SurfaceColumn channelColumn = settlement.Column(0, 0);
	const SurfaceColumn bankColumn = settlement.Column(60, 60);
	REQUIRE(channelColumn.fluid == FluidOrigin::River);
	CHECK(channelColumn.fluidLevelMeters == doctest::Approx(water.surfaceMeters));
	CHECK(channelColumn.fluidDepthMeters > 0.0);
	CHECK(bankColumn.fluid == FluidOrigin::None);
	CHECK(bankColumn.heightBlocks > channelColumn.heightBlocks);
}

TEST_CASE("WorldRiversFollowTheMeanderControlPoint")
{
	const PlanetMap &planet = SharedMap();
	const CartesianPoint3D anchor = Place(35.0, 80.0);
	const LocalGrid grid(kRadius, anchor);

	std::vector<RegionTile> tiles(static_cast<std::size_t>(kRegionTilesPerWorldSide) *
	                              static_cast<std::size_t>(kRegionTilesPerWorldSide));
	for (RegionTile &tile : tiles)
	{
		tile.elevationMeters = 100.0f;
		tile.temperatureCelsius = 15.0f;
		tile.precipitationMm = 1000.0f;
		tile.flags = 0;
	}
	RegionTile &river = tiles[WorldMap::Index({0, 0})];
	SetFlag(river.flags, RegionTileFlag::River);
	river.riverFirst = 0;
	river.riverCount = 1;
	river.flowEast = 1;
	std::vector<RiverChannel> channels{RiverChannel{0.0f, 0.5f, 1.0f, 0.5f, 20.0f, 2.0f, 0.5f, 0.8f}};
	const auto world = std::make_shared<const WorldMap>(grid, std::move(tiles), std::move(channels));
	const WorldTerrain terrain(planet, world);

	// The channel belongs to tile (0, 0): local (0.25, 0.6125) is global
	// (-0.25, +0.1125) tiles from the anchor.
	const GeographicCoordinates onCurve = PlanetaryCoordinates::ToGeographic(
	    grid.Direction(-0.25 * kRegionTileSizeMeters, 0.1125 * kRegionTileSizeMeters));
	const GeographicCoordinates offCurve =
	    PlanetaryCoordinates::ToGeographic(grid.Direction(-0.25 * kRegionTileSizeMeters, 0.0 * kRegionTileSizeMeters));
	const LocalWater wet = terrain.WaterAt(onCurve, 2.0);
	const LocalWater dry = terrain.WaterAt(offCurve, 2.0);
	REQUIRE(wet.origin == FluidOrigin::River);
	CHECK(dry.origin == FluidOrigin::None);
}

TEST_CASE("WorldLakesFillLocalDepressionsToTheSpillLevel")
{
	const PlanetMap &planet = SharedMap();
	const CartesianPoint3D anchor = Place(35.0, 80.0);
	const LocalGrid grid = LocalGrid(kRadius, anchor);

	std::vector<RegionTile> tiles(static_cast<std::size_t>(kRegionTilesPerWorldSide) *
	                              static_cast<std::size_t>(kRegionTilesPerWorldSide));
	for (RegionTile &tile : tiles)
	{
		tile.elevationMeters = 100.0f;
		tile.temperatureCelsius = 15.0f;
		tile.precipitationMm = 1000.0f;
		tile.flags = 0;
	}
	RegionTile &lake = tiles[WorldMap::Index({0, 0})];
	SetFlag(lake.flags, RegionTileFlag::Lake);
	lake.waterLevelMeters = 0.0f;

	// Find the local ground at the tile centre, then put the lake just above
	// its top block so the centre is genuinely under water.
	const auto probeWorld = std::make_shared<const WorldMap>(grid, tiles);
	const WorldTerrain probe(planet, probeWorld);
	const GeographicCoordinates center = grid.Center({0, 0});
	const double ground = probe.ElevationMeters(center, 2.0);
	lake.waterLevelMeters = static_cast<float>(std::floor(ground) + 1.25);
	lake.waterDepthMeters = lake.waterLevelMeters - lake.elevationMeters;

	const auto world = std::make_shared<const WorldMap>(grid, std::move(tiles));
	const RegionGenerator generator(planet, world);
	const WorldTerrain &terrain = generator.GetTerrain();
	const LocalWater water = terrain.WaterAt(center, 2.0);
	REQUIRE(water.origin == FluidOrigin::Lake);
	CHECK(water.surfaceMeters == doctest::Approx(lake.waterLevelMeters));

	const SettlementTerrain settlement(terrain, grid, 0.0, 0.0);
	const SurfaceColumn column = settlement.Column(0, 0);
	REQUIRE(column.fluid == FluidOrigin::Lake);
	CHECK(column.fluidLevelMeters == doctest::Approx(water.surfaceMeters));
	CHECK(column.fluidDepthMeters > 0.0);
}

TEST_CASE("NeighbouringRegionsShareBorderElevationOnAllSides")
{
	const RegionGenerator generator(SharedMap());
	const int edge = LocalGrid::kCellsPerRegionEdge;

	for (const GeographicCoordinates anchor : {GeographicCoordinates{20.0, 200.0}, {70.0, 359.99}, {-89.0, 10.0}})
	{
		const LocalGrid grid = generator.MakeGrid(Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
		const RegionId id{1, -2};
		const RegionData a = generator.Generate(grid, id);
		const RegionData east = generator.Generate(grid, {id.i + 1, id.j});
		const RegionData north = generator.Generate(grid, {id.i, id.j + 1});
		for (int k = 0; k <= edge; ++k)
		{
			Require(a.ElevationAt(edge, k) == east.ElevationAt(0, k), "Eastern border differs from the neighbour.");
			Require(a.ElevationAt(k, edge) == north.ElevationAt(k, 0), "Northern border differs from the neighbour.");
		}
	}
}

TEST_CASE("PolesAreAnchorableAndGenerateFiniteTerrain")
{
	const RegionGenerator generator(SharedMap());
	for (const double latitude : {90.0, -90.0})
	{
		const LocalGrid grid = generator.MakeGrid(Place(latitude, 0.0));
		for (const RegionId id : {RegionId{0, 0}, RegionId{3, -2}})
		{
			const RegionData data = generator.Generate(grid, id);
			for (float value : data.elevationMeters)
			{
				Require(std::isfinite(value), "Polar region has a non-finite elevation.");
			}
		}
	}
}

TEST_CASE("RegionElevationTracksThePlanetField")
{
	const RegionGenerator generator(SharedMap());
	const WorldTerrain &terrain = generator.GetTerrain();

	// Averaged over a region, detail must not drift far from the coarse field.
	for (const GeographicCoordinates place : {GeographicCoordinates{10.0, 20.0}, {-40.0, 300.0}, {65.0, 100.0}})
	{
		const CartesianPoint3D direction = Place(place.latitudeDegrees, place.longitudeDegrees);
		const RegionData data = generator.Generate(generator.MakeGrid(direction), {});
		double sum = 0.0;
		for (float value : data.elevationMeters)
		{
			sum += value;
		}
		const double mean = sum / static_cast<double>(data.elevationMeters.size());
		const double coarse = terrain.SamplePlanet(direction).elevationMeters;
		Require(std::abs(mean - coarse) < 1500.0, "Region mean elevation drifted far from the planet field.");
		Require(data.maxElevationMeters >= data.minElevationMeters, "Region elevation range is inverted.");
	}
}

TEST_CASE("FinerWavelengthOnlyAddsDetail")
{
	const RegionGenerator generator(SharedMap());
	const WorldTerrain &terrain = generator.GetTerrain();
	const CartesianPoint3D direction = Place(30.0, 45.0);
	const double coarse = terrain.ElevationMeters(direction, 256.0);
	const double fine = terrain.ElevationMeters(direction, 1.0);
	// Remaining octaves sum to at most their own amplitude, well below the full
	// detail range.
	Require(std::abs(fine - coarse) < 50.0, "Fine detail rescaled the coarse terrain.");
}

TEST_CASE("PlanetFieldsAreContinuousAcrossCellBorders")
{
	const RegionGenerator generator(SharedMap());
	const WorldTerrain &terrain = generator.GetTerrain();
	const PlanetMap &map = SharedMap();
	const int latitudeBands = map.GetLatitudeBandCount();
	const int longitudeSegments = map.GetLongitudeSegmentCount();

	// Find a border where the neighbouring cells really differ, so the test
	// is not satisfied by a uniform map.
	double largestCellJump = 0.0;
	double largestSampleJump = 0.0;
	for (int latitude = 1; latitude < latitudeBands - 1; ++latitude)
	{
		const PlanetRegion &row = map.GetRegion(latitude, 0);
		for (int longitude = 0; longitude < longitudeSegments; ++longitude)
		{
			const PlanetRegion &west = map.GetRegion(latitude, longitude);
			const PlanetRegion &east = map.GetRegion(latitude, (longitude + 1) % longitudeSegments);
			largestCellJump = std::max(largestCellJump, std::abs(west.temperatureCelsius - east.temperatureCelsius));

			const double borderLongitude = static_cast<double>(longitude + 1) / longitudeSegments * 360.0;
			constexpr double epsilon = 1e-6;
			const PlanetFieldSample before = terrain.SampleAt({row.latitudeDegrees, borderLongitude - epsilon});
			const PlanetFieldSample after = terrain.SampleAt({row.latitudeDegrees, borderLongitude + epsilon});
			largestSampleJump = std::max(
			    largestSampleJump, std::abs(before.surface.temperatureCelsius - after.surface.temperatureCelsius));
		}
	}

	Require(largestCellJump > 0.5, "The test map has no climate difference between neighbouring cells.");
	Require(largestSampleJump < 0.01, "Climate fields still step at a planet cell border.");
}

TEST_CASE("ProvinceContactIsContinuousAcrossCellBorders")
{
	const RegionGenerator generator(SharedMap());
	const WorldTerrain &terrain = generator.GetTerrain();
	const PlanetMap &map = SharedMap();
	const int latitudeBands = map.GetLatitudeBandCount();
	const int longitudeSegments = map.GetLongitudeSegmentCount();

	int tested = 0;
	for (int latitude = 10; latitude < latitudeBands - 10 && tested < 8; ++latitude)
	{
		for (int longitude = 0; longitude < longitudeSegments && tested < 8; ++longitude)
		{
			const ProvinceId west = map.GetRegion(latitude, longitude).provinceId;
			const ProvinceId east = map.GetRegion(latitude, (longitude + 1) % longitudeSegments).provinceId;
			if (west == east)
			{
				continue;
			}
			const PlanetRegion &center = map.GetRegion(latitude, 0);
			const double borderLongitude = static_cast<double>(longitude + 1) / longitudeSegments * 360.0;
			constexpr double epsilon = 1e-9;
			const ProvinceId before =
			    terrain.SampleAt({center.latitudeDegrees, borderLongitude - epsilon}).surface.province;
			// A tiny offset keeps the threshold difference below one part in a
			// million of the transition width.
			const ProvinceId after =
			    terrain.SampleAt({center.latitudeDegrees, borderLongitude + epsilon}).surface.province;
			Require(before == after, "The province jumps exactly at a planet-cell border.");
			++tested;
		}
	}
	Require(tested > 0, "The test planet has no province border.");
}

TEST_CASE("ProvinceContactMixesTheTwoRockPiles")
{
	const RegionGenerator generator(SharedMap());
	const WorldTerrain &terrain = generator.GetTerrain();
	const PlanetMap &map = SharedMap();
	const int latitudeBands = map.GetLatitudeBandCount();
	const int longitudeSegments = map.GetLongitudeSegmentCount();

	bool foundMix = false;
	for (int latitude = 10; latitude < latitudeBands - 10 && !foundMix; ++latitude)
	{
		const PlanetRegion &cell = map.GetRegion(latitude, 0);
		const double metersPerDegree = 111320.0 * std::cos(cell.latitudeDegrees * std::numbers::pi / 180.0);
		for (int longitude = 0; longitude < longitudeSegments && !foundMix; ++longitude)
		{
			if (map.GetRegion(latitude, longitude).provinceId ==
			    map.GetRegion(latitude, (longitude + 1) % longitudeSegments).provinceId)
			{
				continue;
			}
			const double borderLongitude = static_cast<double>(longitude + 1) / longitudeSegments * 360.0;
			for (int offset = -500; offset <= 500; offset += 25)
			{
				const PlanetFieldSample sample =
				    terrain.SampleAt({cell.latitudeDegrees, borderLongitude + offset / metersPerDegree});
				if (sample.surface.secondaryWeight > 0.0 && sample.surface.secondaryProvince != kNoProvince)
				{
					foundMix = true;
					break;
				}
			}
		}
	}
	Require(foundMix, "No mixed province contact zone was found.");

	int borrowed = 0;
	int primary = 0;
	for (int east = 0; east < 512; ++east)
	{
		for (int north = 0; north < 512; ++north)
		{
			(UseSecondaryRock(0.5, east, north, 100) ? borrowed : primary) += 1;
		}
	}
	Require(borrowed > 0 && primary > 0, "The contact mixture does not borrow from both piles.");
}

TEST_CASE("TerrainHasNoCliffWhereTheCoarseFieldCrossesSeaLevel")
{
	const RegionGenerator generator(SharedMap());
	const WorldTerrain &terrain = generator.GetTerrain();

	// Walk along a parallel in coarse steps until the planet field changes sign.
	int crossings = 0;
	for (const double latitude : {-40.0, -10.0, 15.0, 45.0})
	{
		const double coarseStep =
		    20000.0 / (kRadius * std::cos(latitude * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
		for (double longitude = 0.0; longitude < 360.0 && crossings < 12; longitude += coarseStep)
		{
			const double before = terrain.SamplePlanet(Place(latitude, longitude)).elevationMeters;
			const double after = terrain.SamplePlanet(Place(latitude, longitude + coarseStep)).elevationMeters;
			if ((before < 0.0) == (after < 0.0))
			{
				continue;
			}
			++crossings;

			// Fine walk across the crossing, 8 m steps, at the resolution of a region
			// lattice.
			const double fineStep =
			    8.0 / (kRadius * std::cos(latitude * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
			const int steps = static_cast<int>(coarseStep / fineStep);
			double previous = terrain.ElevationMeters(Place(latitude, longitude), 64.0);
			double largestStep = 0.0;
			for (int i = 1; i <= steps; ++i)
			{
				const double h = terrain.ElevationMeters(Place(latitude, longitude + i * fineStep), 64.0);
				largestStep = std::max(largestStep, std::abs(h - previous));
				previous = h;
			}
			Require(largestStep < 12.0, "Terrain has a cliff where the coarse field crosses sea level.");
		}
	}
	Require(crossings > 0, "Found no coastline to test.");
}

TEST_CASE("PatchIsTheGridsOwnLattice")
{
	const RegionGenerator generator(SharedMap());
	const int cells = LocalGrid::kCellsPerRegionEdge;

	for (const GeographicCoordinates anchor : {GeographicCoordinates{35.0, 100.0}, {5.0, 359.99}, {89.0, 10.0}})
	{
		const LocalGrid grid = generator.MakeGrid(Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
		const RegionId id{2, -1};
		const RegionPatch patch = generator.GeneratePatch(grid, id, 1);
		const RegionData region = generator.Generate(grid, id);
		Require(patch.points == 3 * cells + 1, "Patch has the wrong size.");

		// The patch centre is the region's centre, and its middle region is the
		// region itself, point for point.
		const double middle = (patch.points - 1) * 0.5;
		Require(Distance(patch.Direction(middle, middle), grid.CenterDirection(id)) < 1e-9,
		        "Patch is not centred on the region.");
		for (int k = 0; k <= cells; k += 16)
		{
			Require(patch.ElevationAt(cells + k, cells + 7) == region.ElevationAt(k, 7),
			        "Patch centre differs from the region lattice.");
		}

		for (int k = 0; k <= 2 * cells; k += 32)
		{
			const double x = patch.points * 0.3 + k * 0.1;
			const double y = patch.points * 0.6 - k * 0.2;
			double px = 0.0;
			double py = 0.0;
			patch.Project(patch.Direction(x, y), px, py);
			Require(std::abs(px - x) < 1e-3 && std::abs(py - y) < 1e-3, "Patch projection does not invert Direction.");
		}
	}

	bool invalidRadiusRejected = false;
	try
	{
		generator.GeneratePatch(generator.MakeGrid(Place(0.0, 0.0)), {}, 9);
	}
	catch (const std::invalid_argument &)
	{
		invalidRadiusRejected = true;
	}
	Require(invalidRadiusRejected, "An oversized patch radius was accepted.");

	bool farRegionRejected = false;
	try
	{
		generator.Generate(generator.MakeGrid(Place(0.0, 0.0)), {LocalGrid::kMaxRegionIndex + 1, 0});
	}
	catch (const std::out_of_range &)
	{
		farRegionRejected = true;
	}
	Require(farRegionRejected, "A region beyond the grid limit was accepted.");
}

TEST_CASE("CoarseRegionPatchesKeepTheRegionLattice")
{
	const RegionGenerator generator(SharedMap());
	const LocalGrid grid = generator.MakeGrid(Place(35.0, 100.0));
	const RegionPatch patch = generator.GeneratePatch(grid, {2, -1}, 4, 32.0);
	CHECK(patch.cellSizeMeters == doctest::Approx(32.0));
	CHECK(patch.points == (2 * 4 + 1) * 24 + 1);
	CHECK(Distance(patch.Direction((patch.points - 1) * 0.5, (patch.points - 1) * 0.5), grid.CenterDirection({2, -1})) <
	      1e-9);
	bool finite = true;
	for (const float elevation : patch.elevationMeters)
	{
		finite = finite && std::isfinite(elevation);
	}
	CHECK(finite);
}

TEST_CASE("RegionCacheEvictsLeastRecentlyUsed")
{
	const RegionGenerator generator(SharedMap());
	RegionCache cache(generator, generator.MakeGrid(Place(10.0, 10.0)), 2);
	const RegionId a{0, 0};
	const RegionId b{1, 0};
	const RegionId c{2, 0};

	const auto first = cache.Get(a);
	cache.Get(b);
	Require(cache.Get(a) == first, "Cache returned a different instance for a cached region.");
	cache.Get(c); // evicts b
	Require(cache.Size() == 2, "Cache exceeded its capacity.");
	Require(cache.Get(a) == first, "Most recently used region was evicted.");
}

TEST_CASE("SettlementTerrainClassifiesTheSurface")
{
	using K = SurfaceKind;
	Require(SettlementTerrain::ClassifySurface(Biome::HotDesert, 300.0) == K::Sand, "Desert is not sand.");
	Require(SettlementTerrain::ClassifySurface(Biome::TropicalRainforest, 300.0) == K::Grass,
	        "Rainforest is not grass.");
	Require(SettlementTerrain::ClassifySurface(Biome::AlpineRock, 3000.0) == K::Stone, "Alpine rock is not stone.");
	Require(SettlementTerrain::ClassifySurface(Biome::Tundra, 100.0) == K::Dirt, "Tundra is not dirt.");
	Require(SettlementTerrain::ClassifySurface(Biome::TemperateDeciduous, 1.0) == K::Sand, "A beach is not sand.");
	Require(SettlementTerrain::ClassifySurface(Biome::DeepOcean, -500.0) == K::Sand, "The sea floor is not sand.");
}

TEST_CASE("SettlementTerrainColumnsAreStableAndContinuous")
{
	const RegionGenerator generator(SharedMap());

	for (const GeographicCoordinates anchor :
	     {GeographicCoordinates{35.0, 100.0}, {-9.8, 126.8}, {70.0, 20.0}, {90.0, 0.0}})
	{
		const LocalGrid grid = generator.MakeGrid(Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
		const SettlementTerrain terrain(generator.GetTerrain(), grid, 0.0, 0.0);

		// The origin column defines ground level.
		Require(terrain.Column(0, 0).heightBlocks == 0, "The origin column is not at ground level.");
		Require(terrain.SeaLevelBlocks() == -terrain.OriginElevationMeters(),
		        "Sea level is inconsistent with the origin.");

		// Deterministic, and neighbouring columns differ by little.
		int largestStep = 0;
		for (int x = -128; x < 128; x += 5)
		{
			for (int z = -128; z < 128; z += 5)
			{
				const SurfaceColumn column = terrain.Column(x, z);
				Require(column.heightBlocks == terrain.Column(x, z).heightBlocks,
				        "Column height is not deterministic.");
				largestStep =
				    std::max(largestStep, std::abs(column.heightBlocks - terrain.Column(x + 1, z).heightBlocks));
				largestStep =
				    std::max(largestStep, std::abs(column.heightBlocks - terrain.Column(x, z + 1).heightBlocks));
			}
		}
		Require(largestStep <= 6, "Neighbouring block columns differ too much in height.");
	}
}

TEST_CASE("SettlementTerrainAcceptsAPrecomputedFrame")
{
	const RegionGenerator generator(SharedMap());
	const LocalGrid grid = generator.MakeGrid(Place(20.0, 200.0));
	const double originEast = 2.0 * LocalGrid::kRegionSizeMeters;
	const double originNorth = -1.0 * LocalGrid::kRegionSizeMeters;

	const SettlementTerrain fromGrid(generator.GetTerrain(), grid, originEast, originNorth);
	const LocalFrame frame =
	    LocalFrame::At(PlanetaryCoordinates::ToGeographic(grid.Direction(originEast, originNorth)));
	const SettlementTerrain fromFrame(generator.GetTerrain(), frame, originEast, originNorth);

	for (const auto [east, north] : {std::pair{0, 0}, std::pair{-37, 18}, std::pair{64, -52}})
	{
		const SurfaceColumn a = fromGrid.Column(east, north);
		const SurfaceColumn b = fromFrame.Column(east, north);
		Require(a.heightBlocks == b.heightBlocks && a.kind == b.kind && a.province == b.province &&
		            a.fluid == b.fluid && a.elevationMeters == b.elevationMeters,
		        "The precomputed frame and the grid origin disagree.");
	}
}

TEST_CASE("SettlementTerrainFindsTheSea")
{
	const RegionGenerator generator(SharedMap());
	const PlanetMap &map = SharedMap();

	bool foundOcean = false;
	bool foundHighland = false;
	for (int latitude = 10; latitude < map.GetLatitudeBandCount() - 10 && !(foundOcean && foundHighland); ++latitude)
	{
		for (int longitude = 0; longitude < map.GetLongitudeSegmentCount(); ++longitude)
		{
			const PlanetRegion &cell = map.GetRegion(latitude, longitude);
			const CartesianPoint3D center = map.GetPlanet().GetSurfacePoint(latitude, longitude);
			const LocalGrid grid = generator.MakeGrid(center);
			const SettlementTerrain terrain(generator.GetTerrain(), grid, 0.0, 0.0);
			if (!foundOcean && cell.elevationNormalized < -0.4)
			{
				foundOcean = true;
				const SurfaceColumn column = terrain.Column(0, 0);
				Require(column.fluid == FluidOrigin::Sea && column.kind == SurfaceKind::Sand,
				        "Ocean anchor is not under the sea.");
				Require(terrain.SeaLevelBlocks() > 0, "Sea level is not above an ocean anchor.");
			}
			if (!foundHighland && cell.elevationNormalized > 0.1)
			{
				foundHighland = true;
				Require(terrain.Column(0, 0).fluid != FluidOrigin::Sea, "Highland anchor is under the sea.");
				Require(terrain.SeaLevelBlocks() < 0, "Sea level is not below a highland anchor.");
			}
		}
	}
	Require(foundOcean && foundHighland, "The test planet has no ocean or highland.");
}


TEST_CASE("DiagnosticRiverFlooding" * doctest::skip(false))
{
	PlanetParameters parameters;
	parameters.latitudeBands = 90;
	parameters.longitudeSegments = 180;
	parameters.seed = 42;
	const PlanetMap planet(parameters);

	GeographicCoordinates anchor{};
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
			const double score = (region.hasRiver ? 4.0 : 0.0) + 2.0 * region.drainageAccumulation +
			                     region.precipitationMm / 1000.0;
			if (score > bestScore)
			{
				bestScore = score;
				anchor = {region.latitudeDegrees, region.longitudeDegrees};
			}
		}
	}

	auto world = std::make_shared<const WorldMap>(WorldGenerator(planet).Generate(anchor));
	std::printf("\n[DIAG] anchor %.2f %.2f rivers=%zu\n", anchor.latitudeDegrees, anchor.longitudeDegrees,
	            world->Rivers().size());

	RegionTileId riverTile{};
	float widest = 0.0f;
	for (int j = -40; j <= 40; ++j)
	{
		for (int i = -40; i <= 40; ++i)
		{
			const RegionTile &tile = world->At(i, j);
			if (!tile.HasRiver() || tile.IsLake() || tile.IsOcean())
			{
				continue;
			}
			for (const RiverChannel &channel : world->RiversOf(tile))
			{
				if (channel.widthMeters > widest)
				{
					widest = channel.widthMeters;
					riverTile = {i, j};
				}
			}
		}
	}
	const RegionTile &tile = world->At(riverTile);
	std::printf("[DIAG] river tile %d,%d elevation %.1f width %.1f depth %.1f channels %u lake %d\n", riverTile.i,
	            riverTile.j, tile.elevationMeters, widest, tile.waterDepthMeters, tile.riverCount,
	            tile.IsLake() ? 1 : 0);

	const WorldTerrain terrain(planet, world);
	const LocalGrid grid(planet.GetPlanet().GetRadius(), PlanetaryCoordinates::ToCartesian(anchor));
	const SettlementTerrain settlement(terrain, grid, riverTile.i * kRegionTileSizeMeters,
	                                   riverTile.j * kRegionTileSizeMeters);

	constexpr int kStep = 4;
	constexpr int kHalf = 384;
	int total = 0;
	int wet = 0;
	int dryBelowLevel = 0;
	int wetShore = 0;
	int spillEdges = 0;
	double maxDepth = 0.0;
	double minGround = 1.0e30;
	double maxGround = -1.0e30;
	double minWetLevel = 1.0e30;
	double maxWetLevel = -1.0e30;
	constexpr int kSide = 2 * kHalf / kStep + 1;
	std::vector<int> wetMask(static_cast<std::size_t>(kSide * kSide), 0);
	std::vector<double> groundMask(static_cast<std::size_t>(kSide * kSide), 0.0);
	std::vector<double> wetLevels(static_cast<std::size_t>(kSide * kSide), 0.0);
	for (int z = -kHalf; z <= kHalf; z += kStep)
	{
		for (int x = -kHalf; x <= kHalf; x += kStep)
		{
			const SurfaceColumn column = settlement.Column(x, z);
			const std::size_t index = static_cast<std::size_t>((z + kHalf) / kStep) * kSide + (x + kHalf) / kStep;
			groundMask[index] = column.elevationMeters;
			minGround = std::min(minGround, column.elevationMeters);
			maxGround = std::max(maxGround, column.elevationMeters);
			++total;
			if (column.fluid != FluidOrigin::None)
			{
				++wet;
				wetMask[index] = 1;
				maxDepth = std::max(maxDepth, column.fluidDepthMeters);
				minWetLevel = std::min(minWetLevel, column.fluidLevelMeters);
				maxWetLevel = std::max(maxWetLevel, column.fluidLevelMeters);
				wetLevels[index] = column.fluidLevelMeters;
			}
		}
	}
	for (int z = -kHalf; z <= kHalf; z += kStep)
	{
		for (int x = -kHalf; x <= kHalf; x += kStep)
		{
			const std::size_t index = static_cast<std::size_t>((z + kHalf) / kStep) * kSide + (x + kHalf) / kStep;
			if (wetMask[index] == 0)
			{
				continue;
			}
			for (const auto [dx, dz] : {std::pair{kStep, 0}, std::pair{-kStep, 0}, std::pair{0, kStep}, std::pair{0, -kStep}})
			{
				const int nx = x + dx;
				const int nz = z + dz;
				if (nx < -kHalf || nx > kHalf || nz < -kHalf || nz > kHalf)
				{
					continue;
				}
				const std::size_t n = static_cast<std::size_t>((nz + kHalf) / kStep) * kSide + (nx + kHalf) / kStep;
				if (wetMask[n] == 0)
				{
					++wetShore;
					// The neighbour is dry. If its ground top is below this
					// water level, the fluid step will spill sideways.
					if (groundMask[n] + 1.0 < wetLevels[index])
					{
						++spillEdges;
					}
					if (groundMask[n] + 1.0 < wetLevels[index])
					{
						++dryBelowLevel;
					}
				}
			}
		}
	}
	std::printf("[DIAG] ground %.1f..%.1f wet=%d/%d (%.1f%%) depth max %.2f level %.2f..%.2f shore=%d spill=%d\n",
	            minGround, maxGround, wet, total, 100.0 * wet / total, maxDepth, minWetLevel, maxWetLevel,
	            wetShore, spillEdges);
}
