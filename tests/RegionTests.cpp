#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "planet/PlanetMap.h"
#include "planet/region/CubeSphere.h"
#include "planet/region/RegionGenerator.h"
#include "planet/region/RegionLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#define Require(condition, message)     do { if (!(condition)) { FAIL_CHECK(message); } } while (0)

namespace
{
    using namespace Infinitesia;

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

    constexpr std::array<std::array<int, 2>, 4> kEdgeSteps{ { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } };
}

TEST_CASE("CubeSphereRoundTripsDirections")
{
    for (int face = 0; face < CubeSphere::kFaceCount; ++face)
    {
        for (double u : { 0.0, 0.1, 0.5, 0.93, 1.0 })
        {
            for (double v : { 0.0, 0.27, 0.5, 0.71, 1.0 })
            {
                const CartesianPoint3D direction = CubeSphere::ToDirection({ face, u, v });
                const double length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                                direction.z * direction.z);
                Require(std::abs(length - 1.0) < 1e-12, "Face direction is not a unit vector.");

                // Points on a cube edge may legitimately resolve to the other face,
                // so compare directions rather than face coordinates.
                const CubeFacePoint back = CubeSphere::FromDirection(direction);
                const CartesianPoint3D again = CubeSphere::ToDirection(back);
                Require(Distance(direction, again) < 1e-9, "Cube sphere round trip drifted.");
            }
        }
    }
}

TEST_CASE("RegionLayoutHasRoughlyFourKilometreSquareRegions")
{
    const RegionLayout layout(6371000.0);
    Require(layout.RegionSizeMeters() > 4000.0 && layout.RegionSizeMeters() < 4200.0,
            "Region size is not near 4 km.");
    Require(layout.CellSizeMeters() > 31.0 && layout.CellSizeMeters() < 33.0,
            "Lattice cell size is not near 32 m.");

    // Equal-angle cells shrink toward cube corners but stay within ~1.5x.
    const RegionId center{ 4, layout.RegionsPerEdge() / 2, layout.RegionsPerEdge() / 2 };
    const RegionId corner{ 4, 0, 0 };
    const auto span = [&](RegionId id)
    {
        return Distance(layout.PointDirection(id, 0, 0), layout.PointDirection(id, RegionLayout::kCellsPerRegionEdge, 0)) *
               layout.PlanetRadiusMeters();
    };
    Require(span(center) / span(corner) < 1.6, "Corner regions are too distorted.");
}

TEST_CASE("RegionAtInvertsCenterDirection")
{
    const RegionLayout layout(6371000.0);
    const int last = layout.RegionsPerEdge() - 1;
    for (int face = 0; face < CubeSphere::kFaceCount; ++face)
    {
        for (int i : { 0, 1, last / 2, last - 1, last })
        {
            for (int j : { 0, 1, last / 2, last - 1, last })
            {
                const RegionId id{ face, i, j };
                Require(layout.RegionAt(layout.CenterDirection(id)) == id, "RegionAt does not invert CenterDirection.");
            }
        }
    }
}

TEST_CASE("RegionNeighborsAreSymmetricAcrossFaces")
{
    const RegionLayout layout(6371000.0);
    const int last = layout.RegionsPerEdge() - 1;
    for (int face = 0; face < CubeSphere::kFaceCount; ++face)
    {
        for (int i : { 0, 5, last / 2, last })
        {
            for (int j : { 0, 5, last / 2, last })
            {
                const RegionId id{ face, i, j };
                for (const auto& step : kEdgeSteps)
                {
                    const RegionId neighbor = layout.Neighbor(id, step[0], step[1]);
                    Require(layout.IsValid(neighbor), "Neighbor is outside the planet.");
                    Require(!(neighbor == id), "Neighbor is the region itself.");
                    // Axes rotate across a cube edge, so "back" is whichever of the
                    // neighbour's four steps leads home.
                    bool returnsHome = false;
                    for (const auto& back : kEdgeSteps)
                    {
                        returnsHome = returnsHome || layout.Neighbor(neighbor, back[0], back[1]) == id;
                    }
                    Require(returnsHome, "The neighbour does not list the region as its own neighbour.");

                    const double spacing = Distance(layout.CenterDirection(id), layout.CenterDirection(neighbor)) *
                                           layout.PlanetRadiusMeters();
                    Require(spacing > 0.5 * layout.RegionSizeMeters() && spacing < 1.6 * layout.RegionSizeMeters(),
                            "Neighbour is not adjacent.");
                }
            }
        }
    }
}

TEST_CASE("RegionGenerationIsDeterministic")
{
    const RegionGenerator generator(SharedMap());
    const RegionId id = generator.GetLayout().RegionAt(
        PlanetaryCoordinates::ToCartesian({ 35.0, 80.0 }));
    const RegionData first = generator.Generate(id);
    const RegionData second = generator.Generate(id);
    Require(first.elevationMeters == second.elevationMeters, "Region elevation is not deterministic.");
    Require(first.biome == second.biome, "Region biomes are not deterministic.");
    Require(first.elevationMeters.size() ==
                static_cast<std::size_t>(RegionData::kPoints) * RegionData::kPoints,
            "Region lattice has the wrong size.");
}

TEST_CASE("NeighbouringRegionsShareBorderElevation")
{
    const RegionGenerator generator(SharedMap());
    const RegionLayout& layout = generator.GetLayout();
    const RegionId id = layout.RegionAt(PlanetaryCoordinates::ToCartesian({ 20.0, 200.0 }));
    const int edge = RegionLayout::kCellsPerRegionEdge;

    const RegionData a = generator.Generate(id);
    const RegionData right = generator.Generate(layout.Neighbor(id, 1, 0));
    const RegionData up = generator.Generate(layout.Neighbor(id, 0, 1));

    for (int k = 0; k <= edge; ++k)
    {
        Require(a.ElevationAt(edge, k) == right.ElevationAt(0, k), "Right border differs from neighbour.");
        Require(a.ElevationAt(k, edge) == up.ElevationAt(k, 0), "Upper border differs from neighbour.");
    }
}

TEST_CASE("RegionsAgreeAcrossCubeEdges")
{
    const RegionGenerator generator(SharedMap());
    const RegionLayout& layout = generator.GetLayout();
    const int last = layout.RegionsPerEdge() - 1;
    const int edge = RegionLayout::kCellsPerRegionEdge;

    // Last column of a face, away from corners; its +u neighbour lives on another face.
    const RegionId id{ 4, last, last / 2 };
    const RegionId across = layout.Neighbor(id, 1, 0);
    Require(across.face != id.face, "Expected the neighbour on another face.");

    const RegionData a = generator.Generate(id);
    const RegionData b = generator.Generate(across);

    // Match each shared border point to the closest border point of the other region.
    for (int k = 0; k <= edge; k += 8)
    {
        const CartesianPoint3D direction = layout.PointDirection(id, edge, k);
        double best = std::numeric_limits<double>::max();
        float bestElevation = 0.0f;
        for (int x = 0; x <= edge; ++x)
        {
            for (int y : { 0, edge })
            {
                for (int swap = 0; swap < 2; ++swap)
                {
                    const int px = swap ? y : x;
                    const int py = swap ? x : y;
                    const double distance = Distance(direction, layout.PointDirection(across, px, py));
                    if (distance < best)
                    {
                        best = distance;
                        bestElevation = b.ElevationAt(px, py);
                    }
                }
            }
        }
        Require(best * layout.PlanetRadiusMeters() < 0.01, "No matching lattice point across the cube edge.");
        Require(std::abs(a.ElevationAt(edge, k) - bestElevation) < 0.01f,
                "Elevation differs across a cube edge.");
    }
}

TEST_CASE("RegionElevationTracksThePlanetField")
{
    const RegionGenerator generator(SharedMap());
    const RegionTerrain& terrain = generator.GetTerrain();

    // Averaged over a region, detail must not drift far from the coarse field.
    for (const GeographicCoordinates place : { GeographicCoordinates{ 10.0, 20.0 }, { -40.0, 300.0 }, { 65.0, 100.0 } })
    {
        const CartesianPoint3D direction = PlanetaryCoordinates::ToCartesian(place);
        const RegionData data = generator.Generate(generator.GetLayout().RegionAt(direction));
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
    const CartesianPoint3D direction = PlanetaryCoordinates::ToCartesian({ 30.0, 45.0 });
    const double coarse = terrain.ElevationMeters(direction, 256.0);
    const double fine = terrain.ElevationMeters(direction, 1.0);
    // Remaining octaves sum to at most their own amplitude, well below the full detail range.
    Require(std::abs(fine - coarse) < 50.0, "Fine detail rescaled the coarse terrain.");
}

TEST_CASE("PatchMatchesRegionDataAndCrossesCubeEdges")
{
    const RegionGenerator generator(SharedMap());
    const RegionLayout& layout = generator.GetLayout();
    const int cells = RegionLayout::kCellsPerRegionEdge;

    // Inside a face the centre of the patch is exactly the region's own lattice.
    const RegionId inner{ 4, 100, 100 };
    const RegionData region = generator.Generate(inner);
    const RegionPatch patch = generator.GeneratePatch(inner, 1);
    Require(patch.points == 3 * cells + 1, "Patch has the wrong size.");
    for (int k = 0; k <= cells; k += 16)
    {
        Require(patch.ElevationAt(cells + k, cells + 7) == region.ElevationAt(k, 7),
                "Patch centre differs from the region lattice.");
    }

    // At a cube edge the patch continues onto the next face without a jump.
    const RegionId edge{ 4, layout.RegionsPerEdge() - 1, layout.RegionsPerEdge() / 2 };
    const RegionPatch across = generator.GeneratePatch(edge, 1);
    double largestStep = 0.0;
    for (int x = 1; x < across.points; ++x)
    {
        largestStep = std::max<double>(largestStep, std::abs(across.ElevationAt(x, across.points / 2) -
                                                             across.ElevationAt(x - 1, across.points / 2)));
    }
    Require(largestStep < 40.0, "Patch has a jump where it crosses a cube edge.");

    bool invalidRadiusRejected = false;
    try { generator.GeneratePatch(inner, 9); } catch (const std::invalid_argument&) { invalidRadiusRejected = true; }
    Require(invalidRadiusRejected, "An oversized patch radius was accepted.");
}

TEST_CASE("RegionCacheEvictsLeastRecentlyUsed")
{
    const RegionGenerator generator(SharedMap());
    RegionCache cache(generator, 2);
    const RegionLayout& layout = generator.GetLayout();
    const RegionId a{ 0, 10, 10 };
    const RegionId b = layout.Neighbor(a, 1, 0);
    const RegionId c = layout.Neighbor(a, 0, 1);

    const auto first = cache.Get(a);
    cache.Get(b);
    Require(cache.Get(a) == first, "Cache returned a different instance for a cached region.");
    cache.Get(c); // evicts b
    Require(cache.Size() == 2, "Cache exceeded its capacity.");
    Require(cache.Get(a) == first, "Most recently used region was evicted.");
}
