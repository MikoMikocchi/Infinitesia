#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "planet/PlanetMap.h"
#include "planet/region/LocalGrid.h"
#include "planet/region/RegionGenerator.h"
#include "planet/region/SettlementTerrain.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#define Require(condition, message)     do { if (!(condition)) { FAIL_CHECK(message); } } while (0)

namespace
{
    using namespace Infinitesia;

    constexpr double kRadius = 6371000.0;

    // A coarse planet keeps these tests fast in unoptimized builds.
    const PlanetMap& SharedMap()
    {
        static const PlanetMap map = []
        {
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
        return PlanetaryCoordinates::ToCartesian({ latitude, longitude });
    }

    double EastwardDegrees(double from, double to)
    {
        double delta = to - from;
        if (delta > 180.0) delta -= 360.0;
        if (delta < -180.0) delta += 360.0;
        return delta;
    }

    // Anchors spread over the planet: tropics, mid-latitudes, the longitude seam and both poles.
    const GeographicCoordinates kAnchors[] = {
        { 0.0, 0.0 }, { 35.0, 100.0 }, { -50.0, 10.0 }, { 70.0, 359.99 }, { -9.8, 126.8 },
        { 89.0, 40.0 }, { -90.0, 0.0 }, { 90.0, 0.0 }
    };
}

TEST_CASE("RegionsAreEqualSquaresOfFourKilometres")
{
    static_assert(LocalGrid::kRegionSizeMeters == 4096.0);
    static_assert(LocalGrid::kCellSizeMeters == 32.0);

    for (const GeographicCoordinates anchor : kAnchors)
    {
        const LocalGrid grid(kRadius, Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
        for (const RegionId id : { RegionId{ 0, 0 }, RegionId{ 3, -2 }, RegionId{ -20, 14 }, RegionId{ 40, 40 } })
        {
            // Side lengths of the region measured along the surface.
            const double east = Distance(grid.PointDirection(id, 0, 0), grid.PointDirection(id, 128, 0)) * kRadius;
            const double north = Distance(grid.PointDirection(id, 0, 0), grid.PointDirection(id, 0, 128)) * kRadius;
            // Within ~160 km of the anchor the projection stretches by less than 0.1%.
            Require(std::abs(east - 4096.0) < 5.0 && std::abs(north - 4096.0) < 5.0,
                    "Region is not a 4 km square near the anchor.");
        }
    }
}

TEST_CASE("RegionsFormAChessboard")
{
    for (const GeographicCoordinates anchor : kAnchors)
    {
        const LocalGrid grid(kRadius, Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
        for (const RegionId id : { RegionId{ 0, 0 }, RegionId{ 5, -7 }, RegionId{ -30, 22 } })
        {
            const int edge = LocalGrid::kCellsPerRegionEdge;
            for (int k = 0; k <= edge; k += 16)
            {
                // Each side meets exactly one neighbour, point for point.
                Require(Distance(grid.PointDirection(id, edge, k), grid.PointDirection({ id.i + 1, id.j }, 0, k)) < 1e-12,
                        "Eastern border does not meet the eastern neighbour.");
                Require(Distance(grid.PointDirection(id, 0, k), grid.PointDirection({ id.i - 1, id.j }, edge, k)) < 1e-12,
                        "Western border does not meet the western neighbour.");
                Require(Distance(grid.PointDirection(id, k, edge), grid.PointDirection({ id.i, id.j + 1 }, k, 0)) < 1e-12,
                        "Northern border does not meet the northern neighbour.");
                Require(Distance(grid.PointDirection(id, k, 0), grid.PointDirection({ id.i, id.j - 1 }, k, edge)) < 1e-12,
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
        for (const RegionId id : { RegionId{ 0, 0 }, RegionId{ 1, 0 }, RegionId{ 0, -1 }, RegionId{ -12, 9 }, RegionId{ 100, 100 } })
        {
            Require(grid.RegionAt(grid.CenterDirection(id)) == id, "RegionAt does not invert CenterDirection.");
        }
        for (const double east : { -150000.0, -4096.0, 0.0, 3000.5, 90000.0 })
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
    const RegionId outermost{ LocalGrid::kMaxRegionIndex, 0 };
    const double side = Distance(grid.PointDirection(outermost, 0, 0), grid.PointDirection(outermost, 128, 0)) * kRadius;
    Require(side < 4096.0 && side > 4096.0 * 0.9, "Distance stretch at the grid limit is out of range.");
    Require(LocalGrid::IsValid(outermost) && !LocalGrid::IsValid({ LocalGrid::kMaxRegionIndex + 1, 0 }),
            "Grid limit is not enforced by IsValid.");
}

TEST_CASE("RegionGenerationIsDeterministic")
{
    const RegionGenerator generator(SharedMap());
    const LocalGrid grid = generator.MakeGrid(Place(35.0, 80.0));
    const RegionData first = generator.Generate(grid, { 2, -1 });
    const RegionData second = generator.Generate(grid, { 2, -1 });
    Require(first.elevationMeters == second.elevationMeters, "Region elevation is not deterministic.");
    Require(first.biome == second.biome, "Region biomes are not deterministic.");
    Require(first.elevationMeters.size() ==
                static_cast<std::size_t>(RegionData::kPoints) * RegionData::kPoints,
            "Region lattice has the wrong size.");
}

TEST_CASE("NeighbouringRegionsShareBorderElevationOnAllSides")
{
    const RegionGenerator generator(SharedMap());
    const int edge = LocalGrid::kCellsPerRegionEdge;

    for (const GeographicCoordinates anchor : { GeographicCoordinates{ 20.0, 200.0 }, { 70.0, 359.99 }, { -89.0, 10.0 } })
    {
        const LocalGrid grid = generator.MakeGrid(Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
        const RegionId id{ 1, -2 };
        const RegionData a = generator.Generate(grid, id);
        const RegionData east = generator.Generate(grid, { id.i + 1, id.j });
        const RegionData north = generator.Generate(grid, { id.i, id.j + 1 });
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
    for (const double latitude : { 90.0, -90.0 })
    {
        const LocalGrid grid = generator.MakeGrid(Place(latitude, 0.0));
        for (const RegionId id : { RegionId{ 0, 0 }, RegionId{ 3, -2 } })
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
    const RegionTerrain& terrain = generator.GetTerrain();

    // Averaged over a region, detail must not drift far from the coarse field.
    for (const GeographicCoordinates place : { GeographicCoordinates{ 10.0, 20.0 }, { -40.0, 300.0 }, { 65.0, 100.0 } })
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
    const RegionTerrain& terrain = generator.GetTerrain();
    const CartesianPoint3D direction = Place(30.0, 45.0);
    const double coarse = terrain.ElevationMeters(direction, 256.0);
    const double fine = terrain.ElevationMeters(direction, 1.0);
    // Remaining octaves sum to at most their own amplitude, well below the full detail range.
    Require(std::abs(fine - coarse) < 50.0, "Fine detail rescaled the coarse terrain.");
}

TEST_CASE("TerrainHasNoCliffWhereTheCoarseFieldCrossesSeaLevel")
{
    const RegionGenerator generator(SharedMap());
    const RegionTerrain& terrain = generator.GetTerrain();

    // Walk along a parallel in coarse steps until the planet field changes sign.
    int crossings = 0;
    for (const double latitude : { -40.0, -10.0, 15.0, 45.0 })
    {
        const double coarseStep = 20000.0 / (kRadius * std::cos(latitude * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
        for (double longitude = 0.0; longitude < 360.0 && crossings < 12; longitude += coarseStep)
        {
            const double before = terrain.SamplePlanet(Place(latitude, longitude)).elevationMeters;
            const double after = terrain.SamplePlanet(Place(latitude, longitude + coarseStep)).elevationMeters;
            if ((before < 0.0) == (after < 0.0))
            {
                continue;
            }
            ++crossings;

            // Fine walk across the crossing, 8 m steps, at the resolution of a region lattice.
            const double fineStep = 8.0 / (kRadius * std::cos(latitude * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
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

    for (const GeographicCoordinates anchor : { GeographicCoordinates{ 35.0, 100.0 }, { 5.0, 359.99 }, { 89.0, 10.0 } })
    {
        const LocalGrid grid = generator.MakeGrid(Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
        const RegionId id{ 2, -1 };
        const RegionPatch patch = generator.GeneratePatch(grid, id, 1);
        const RegionData region = generator.Generate(grid, id);
        Require(patch.points == 3 * cells + 1, "Patch has the wrong size.");

        // The patch centre is the region's centre, and its middle region is the region itself, point for point.
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
    try { generator.GeneratePatch(generator.MakeGrid(Place(0.0, 0.0)), {}, 9); }
    catch (const std::invalid_argument&) { invalidRadiusRejected = true; }
    Require(invalidRadiusRejected, "An oversized patch radius was accepted.");

    bool farRegionRejected = false;
    try { generator.Generate(generator.MakeGrid(Place(0.0, 0.0)), { LocalGrid::kMaxRegionIndex + 1, 0 }); }
    catch (const std::out_of_range&) { farRegionRejected = true; }
    Require(farRegionRejected, "A region beyond the grid limit was accepted.");
}

TEST_CASE("RegionCacheEvictsLeastRecentlyUsed")
{
    const RegionGenerator generator(SharedMap());
    RegionCache cache(generator, generator.MakeGrid(Place(10.0, 10.0)), 2);
    const RegionId a{ 0, 0 };
    const RegionId b{ 1, 0 };
    const RegionId c{ 2, 0 };

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
    Require(SettlementTerrain::ClassifySurface(Biome::TropicalRainforest, 300.0) == K::Grass, "Rainforest is not grass.");
    Require(SettlementTerrain::ClassifySurface(Biome::AlpineRock, 3000.0) == K::Stone, "Alpine rock is not stone.");
    Require(SettlementTerrain::ClassifySurface(Biome::Tundra, 100.0) == K::Dirt, "Tundra is not dirt.");
    Require(SettlementTerrain::ClassifySurface(Biome::TemperateDeciduous, 1.0) == K::Sand, "A beach is not sand.");
    Require(SettlementTerrain::ClassifySurface(Biome::DeepOcean, -500.0) == K::Sand, "The sea floor is not sand.");
}

TEST_CASE("SettlementTerrainColumnsAreStableAndContinuous")
{
    const RegionGenerator generator(SharedMap());

    for (const GeographicCoordinates anchor : { GeographicCoordinates{ 35.0, 100.0 }, { -9.8, 126.8 }, { 70.0, 20.0 },
                                                { 90.0, 0.0 } })
    {
        const LocalGrid grid = generator.MakeGrid(Place(anchor.latitudeDegrees, anchor.longitudeDegrees));
        const SettlementTerrain terrain(generator.GetTerrain(), grid, 0.0, 0.0);

        // The origin column defines ground level.
        Require(terrain.Column(0, 0).heightBlocks == 0, "The origin column is not at ground level.");
        Require(terrain.SeaLevelBlocks() == -terrain.OriginElevationMeters(), "Sea level is inconsistent with the origin.");

        // Deterministic, and neighbouring columns differ by little.
        int largestStep = 0;
        for (int x = -128; x < 128; x += 5)
        {
            for (int z = -128; z < 128; z += 5)
            {
                const SurfaceColumn column = terrain.Column(x, z);
                Require(column.heightBlocks == terrain.Column(x, z).heightBlocks, "Column height is not deterministic.");
                largestStep = std::max(largestStep, std::abs(column.heightBlocks - terrain.Column(x + 1, z).heightBlocks));
                largestStep = std::max(largestStep, std::abs(column.heightBlocks - terrain.Column(x, z + 1).heightBlocks));
            }
        }
        Require(largestStep <= 6, "Neighbouring block columns differ too much in height.");
    }
}

TEST_CASE("SettlementTerrainFindsTheSea")
{
    const RegionGenerator generator(SharedMap());
    const PlanetMap& map = SharedMap();

    bool foundOcean = false;
    bool foundHighland = false;
    for (int latitude = 10; latitude < map.GetLatitudeBandCount() - 10 && !(foundOcean && foundHighland); ++latitude)
    {
        for (int longitude = 0; longitude < map.GetLongitudeSegmentCount(); ++longitude)
        {
            const PlanetRegion& cell = map.GetRegion(latitude, longitude);
            const CartesianPoint3D center = map.GetPlanet().GetSurfacePoint(latitude, longitude);
            const LocalGrid grid = generator.MakeGrid(center);
            const SettlementTerrain terrain(generator.GetTerrain(), grid, 0.0, 0.0);
            if (!foundOcean && cell.elevationNormalized < -0.4)
            {
                foundOcean = true;
                const SurfaceColumn column = terrain.Column(0, 0);
                Require(column.underSea && column.kind == SurfaceKind::Sand, "Ocean anchor is not under the sea.");
                Require(terrain.SeaLevelBlocks() > 0, "Sea level is not above an ocean anchor.");
            }
            if (!foundHighland && cell.elevationNormalized > 0.1)
            {
                foundHighland = true;
                Require(!terrain.Column(0, 0).underSea, "Highland anchor is under the sea.");
                Require(terrain.SeaLevelBlocks() < 0, "Sea level is not below a highland anchor.");
            }
        }
    }
    Require(foundOcean && foundHighland, "The test planet has no ocean or highland.");
}
