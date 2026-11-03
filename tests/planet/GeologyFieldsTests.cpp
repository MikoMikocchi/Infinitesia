#include <doctest/doctest.h>

#include "planet/geology/PlanetEpochs.h"
#include "support/PlanetGeologyFixtures.h"
#include "support/Require.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

using namespace Infinitesia;
using namespace InfinitesiaTests;

TEST_CASE("GeologicalFieldsAreFiniteAndInRange")
{
    const PlanetMap& map = SharedMap();
    const double planetAgeMyr = map.GetClimateInputs().planetAgeYears / 1.0e6;
    for (int la = 0; la < map.GetLatitudeBandCount(); ++la)
    {
        for (int lo = 0; lo < map.GetLongitudeSegmentCount(); ++lo)
        {
            const PlanetRegion& c = map.GetRegion(la, lo);
            Require(std::isfinite(c.distanceToBoundaryKm) && c.distanceToBoundaryKm >= 0.0,
                    "Distance to the boundary is negative or not finite.");
            Require((c.boundarySubtype == PlateBoundaryKind::None) == (c.distanceToBoundaryKm > kBoundaryReachKm),
                    "Boundary kind is None exactly beyond the reach distance.");
            Require(std::hypot(c.plateVelocityEastCmPerYear, c.plateVelocityNorthCmPerYear) < 25.0,
                    "Plate speed is outside the plausible range.");
            Require(std::hypot(c.boundaryRelativeVelocityEastCmPerYear, c.boundaryRelativeVelocityNorthCmPerYear) < 50.0,
                    "Relative plate speed is outside the plausible range.");
            Require(c.crustAgeMyr >= 0.0 && c.crustAgeMyr <= planetAgeMyr,
                    "Crust age is outside the planet's history.");
            Require(c.sedimentThicknessKm >= 0.02 && c.sedimentThicknessKm <= 14.0,
                    "Sediment thickness is out of range.");
            Require(c.heatFlowMwPerM2 >= 5.0 && c.heatFlowMwPerM2 <= 400.0,
                    "Heat flow is out of range.");
            Require(c.hotspotInfluence >= 0.0 && c.hotspotInfluence <= 1.0,
                    "Hotspot influence is outside 0..1.");
            Require(c.paleoLatitudeDegrees == c.latitudeDegrees,
                    "Paleolatitude is reserved and must equal the latitude.");
            Require(c.provinceId != kNoProvince && c.provinceId <= map.GetProvinces().size(),
                    "Cell has no valid province.");
            Require(!c.isSubductingPlate ||
                    c.boundarySubtype == PlateBoundaryKind::OceanicSubduction ||
                    c.boundarySubtype == PlateBoundaryKind::ContinentalSubduction ||
                    c.distanceToBoundaryKm > kBoundaryReachKm,
                    "Only subduction boundaries have a down-going side.");
        }
    }
}

TEST_CASE("BoundaryDistanceMatchesTheNarrowStressBand")
{
    // tectonicStress is positive only within 1.6 degrees of gap, which is 0.8 degrees
    // (~89 km) from the boundary; the distance field must agree with it.
    const PlanetMap& map = SharedMap();
    const double kmPerDegree = kEarthRadiusKm * std::numbers::pi / 180.0;
    int stressedCells = 0;
    for (int la = 0; la < map.GetLatitudeBandCount(); ++la)
    {
        for (int lo = 0; lo < map.GetLongitudeSegmentCount(); ++lo)
        {
            const PlanetRegion& c = map.GetRegion(la, lo);
            if (c.tectonicStress > 0.0)
            {
                ++stressedCells;
                Require(c.distanceToBoundaryKm <= 0.8 * kmPerDegree * 1.001,
                        "A stressed cell is farther from the boundary than the stress band.");
                Require(c.boundarySubtype != PlateBoundaryKind::None,
                        "A stressed cell has no boundary kind.");
            }
        }
    }
    CHECK(stressedCells > 100);
}

TEST_CASE("ContinentalCrustAgesFollowTheAgeFieldAndHaveCratons")
{
    const PlanetMap& map = SharedMap();
    const double planetAgeMyr = map.GetClimateInputs().planetAgeYears / 1.0e6;

    struct Sample
    {
        double age;
        double ageMyr;
    };
    std::vector<Sample> continental;
    double continentalArea = 0.0;
    double cratonArea = 0.0;
    for (int la = 0; la < map.GetLatitudeBandCount(); ++la)
    {
        for (int lo = 0; lo < map.GetLongitudeSegmentCount(); ++lo)
        {
            const PlanetRegion& c = map.GetRegion(la, lo);
            if (c.continentalCrustFraction > 0.5)
            {
                continental.push_back({ c.crustAge, c.crustAgeMyr });
                continentalArea += CellArea(c);
                if (c.crustAgeMyr > planetAgeMyr * 2.0 / 3.0)
                    cratonArea += CellArea(c);
                Require(c.crustAgeMyr >= 0.05 * planetAgeMyr * 0.99,
                        "Continental crust is younger than the youngest assigned age.");
            }
            else
            {
                Require(std::abs(c.crustAgeMyr - c.crustAge * 180.0) < 1e-9,
                        "Oceanic crust age must follow its normalized age over 180 Myr.");
            }
        }
    }

    // The ranking keeps the spatial pattern: an older field value never gets a younger age.
    std::sort(continental.begin(), continental.end(),
        [](const Sample& a, const Sample& b) { return a.age < b.age; });
    for (std::size_t i = 1; i < continental.size(); ++i)
    {
        if (continental[i].age > continental[i - 1].age)
        {
            Require(continental[i].ageMyr >= continental[i - 1].ageMyr - 1e-9,
                    "Continental age ranking is not monotonic in the age field.");
        }
    }
    const double cratonShare = cratonArea / continentalArea;
    CHECK(cratonShare == doctest::Approx(0.28).epsilon(0.1));
}

TEST_CASE("GeologicalFieldsAreDeterministic")
{
    PlanetParameters parameters;
    parameters.latitudeBands = 60;
    parameters.longitudeSegments = 120;
    parameters.seed = 77;
    const PlanetMap first(parameters);
    const PlanetMap second(parameters);
    REQUIRE(first.GetProvinces().size() == second.GetProvinces().size());
    for (int la = 0; la < parameters.latitudeBands; ++la)
    {
        for (int lo = 0; lo < parameters.longitudeSegments; ++lo)
        {
            const PlanetRegion& a = first.GetRegion(la, lo);
            const PlanetRegion& b = second.GetRegion(la, lo);
            Require(a.provinceId == b.provinceId && a.sedimentThicknessKm == b.sedimentThicknessKm &&
                    a.heatFlowMwPerM2 == b.heatFlowMwPerM2 && a.crustAgeMyr == b.crustAgeMyr &&
                    a.distanceToBoundaryKm == b.distanceToBoundaryKm && a.boundarySubtype == b.boundarySubtype &&
                    a.hotspotInfluence == b.hotspotInfluence &&
                    a.plateVelocityEastCmPerYear == b.plateVelocityEastCmPerYear,
                    "Geological fields are not deterministic for an identical seed.");
        }
    }

    parameters.seed = 78;
    const PlanetMap other(parameters);
    int different = 0;
    for (int la = 0; la < parameters.latitudeBands; ++la)
    {
        for (int lo = 0; lo < parameters.longitudeSegments; ++lo)
        {
            different += first.GetRegion(la, lo).provinceId != other.GetRegion(la, lo).provinceId ? 1 : 0;
        }
    }
    CHECK(different > 0);
}

TEST_CASE("GeologyHandlesCoarseGridsAndSmallPlanets")
{
    for (const auto& [bands, mass] : { std::pair{ 12, 5.9722e24 }, std::pair{ 24, 6.4e23 }, std::pair{ 6, 5.9722e24 } })
    {
        PlanetParameters parameters;
        parameters.latitudeBands = bands;
        parameters.longitudeSegments = bands * 2;
        parameters.massKilograms = mass;
        const PlanetMap map(parameters);
        REQUIRE(!map.GetProvinces().empty());
        for (int la = 0; la < bands; ++la)
        {
            for (int lo = 0; lo < bands * 2; ++lo)
            {
                const PlanetRegion& c = map.GetRegion(la, lo);
                Require(map.FindProvince(c) != nullptr, "A coarse-grid cell has no province.");
                Require(std::isfinite(c.heatFlowMwPerM2) && std::isfinite(c.sedimentThicknessKm),
                        "A coarse-grid cell has a non-finite geological field.");
            }
        }
    }
}
