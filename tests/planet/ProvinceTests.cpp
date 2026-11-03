#include <doctest/doctest.h>

#include "planet/geology/PlanetEpochs.h"
#include "support/PlanetGeologyFixtures.h"
#include "support/Require.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

using namespace Infinitesia;
using namespace InfinitesiaTests;

TEST_CASE("ProvincesPartitionThePlanetIntoConnectedRegions")
{
    const PlanetMap& map = SharedMap();
    const std::vector<Province>& provinces = map.GetProvinces();
    const int latB = map.GetLatitudeBandCount();
    const int lonS = map.GetLongitudeSegmentCount();
    REQUIRE(!provinces.empty());

    // Ids follow the first appearance in a row-major scan.
    CHECK(map.GetRegion(0, 0).provinceId == 1);
    ProvinceId highest = 0;
    std::vector<int> cells(provinces.size() + 1, 0);
    std::vector<std::pair<int, int>> firstCell(provinces.size() + 1, { -1, -1 });
    double totalArea = 0.0;
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            const PlanetRegion& c = map.GetRegion(la, lo);
            Require(c.provinceId <= highest + 1, "Province ids are not in scan order.");
            highest = std::max(highest, c.provinceId);
            ++cells[c.provinceId];
            if (firstCell[c.provinceId].first < 0)
                firstCell[c.provinceId] = { la, lo };
            totalArea += CellArea(c);
        }
    }

    double provinceAreaKm2 = 0.0;
    for (const Province& province : provinces)
    {
        Require(map.FindProvince(province.id) == &province, "FindProvince does not return the table entry.");
        Require(province.cellCount == cells[province.id], "Province cell count does not match the grid.");
        Require(province.cellCount >= 6, "A province is smaller than the minimum size.");
        Require(province.landFraction >= 0.0 && province.landFraction <= 1.0, "Land fraction out of range.");
        Require(province.centroidLatitudeDegrees >= -90.0 && province.centroidLatitudeDegrees <= 90.0 &&
                province.centroidLongitudeDegrees >= 0.0 && province.centroidLongitudeDegrees < 360.0,
                "Province centroid is not a valid position.");
        provinceAreaKm2 += province.areaKm2;

        // Connected: a flood fill from one member reaches all of them.
        std::vector<char> seen(static_cast<std::size_t>(latB) * lonS, 0);
        std::vector<std::pair<int, int>> stack{ firstCell[province.id] };
        seen[static_cast<std::size_t>(stack.back().first) * lonS + stack.back().second] = 1;
        int reached = 0;
        while (!stack.empty())
        {
            const auto [la, lo] = stack.back();
            stack.pop_back();
            ++reached;
            for (int dla = -1; dla <= 1; ++dla)
            {
                for (int dlo = -1; dlo <= 1; ++dlo)
                {
                    const int nla = la + dla;
                    if (nla < 0 || nla >= latB)
                        continue;
                    const int nlo = (lo + dlo + lonS) % lonS;
                    const std::size_t next = static_cast<std::size_t>(nla) * lonS + nlo;
                    if (!seen[next] && map.GetRegion(nla, nlo).provinceId == province.id)
                    {
                        seen[next] = 1;
                        stack.push_back({ nla, nlo });
                    }
                }
            }
        }
        Require(reached == province.cellCount, "A province is not connected.");
    }
    CHECK(highest == provinces.size());

    // The province areas tile the sphere.
    const double sphereKm2 = 4.0 * std::numbers::pi * kEarthRadiusKm * kEarthRadiusKm;
    CHECK(provinceAreaKm2 == doctest::Approx(sphereKm2).epsilon(0.01));
    CHECK(totalArea > 0.0);
}

TEST_CASE("ProvincesFollowThePlateBoundaryContext")
{
    const PlanetMap& map = SharedMap();
    struct Counts
    {
        double area = 0.0;
        double matching = 0.0;
    };
    Counts trench, collision, rift, continentalArc, oceanic;
    for (int la = 0; la < map.GetLatitudeBandCount(); ++la)
    {
        for (int lo = 0; lo < map.GetLongitudeSegmentCount(); ++lo)
        {
            const PlanetRegion& c = map.GetRegion(la, lo);
            const ProvinceType type = map.FindProvince(c)->type;
            const double area = CellArea(c);
            const auto count = [&](Counts& counts, bool matches)
            {
                counts.area += area;
                counts.matching += matches ? area : 0.0;
            };
            const bool subduction = c.boundarySubtype == PlateBoundaryKind::OceanicSubduction ||
                                    c.boundarySubtype == PlateBoundaryKind::ContinentalSubduction;
            switch (type)
            {
            case ProvinceType::Trench: count(trench, subduction && c.continentalCrustFraction <= 0.5); break;
            case ProvinceType::CollisionOrogen: count(collision, c.boundarySubtype == PlateBoundaryKind::ContinentalCollision); break;
            case ProvinceType::RiftSystem: count(rift, c.boundarySubtype == PlateBoundaryKind::ContinentalRift); break;
            case ProvinceType::ContinentalArc: count(continentalArc, c.boundarySubtype == PlateBoundaryKind::ContinentalSubduction); break;
            default: break;
            }
            if (IsOceanicType(type))
                count(oceanic, c.continentalCrustFraction <= 0.5);
        }
    }
    CHECK(trench.area > 0.0);
    CHECK(collision.area > 0.0);
    CHECK(rift.area > 0.0);
    CHECK(continentalArc.area > 0.0);
    CHECK((trench.matching / trench.area > 0.85));
    CHECK((collision.matching / collision.area > 0.85));
    CHECK((rift.matching / rift.area > 0.85));
    CHECK((continentalArc.matching / continentalArc.area > 0.85));
    CHECK((oceanic.matching / oceanic.area > 0.95));
}

TEST_CASE("ProvinceSharesArePlausibleAcrossSeeds")
{
    // Averaged over several planets: the land is mostly platform and craton, with
    // cratons around a fifth of it, active belts a small share, and every class present.
    double land = 0.0;
    std::array<double, kProvinceTypeCount> landByType{};
    std::array<double, kProvinceTypeCount> allByType{};
    for (std::uint32_t seed = 1; seed <= 4; ++seed)
    {
        const PlanetMap map = CoarseMap(seed);
        double seedLand = 0.0;
        double cratonLand = 0.0;
        for (int la = 0; la < map.GetLatitudeBandCount(); ++la)
        {
            for (int lo = 0; lo < map.GetLongitudeSegmentCount(); ++lo)
            {
                const PlanetRegion& c = map.GetRegion(la, lo);
                const auto type = static_cast<std::size_t>(map.FindProvince(c)->type);
                allByType[type] += CellArea(c);
                if (!c.isOcean)
                {
                    landByType[type] += CellArea(c);
                    seedLand += CellArea(c);
                    land += CellArea(c);
                    if (type == static_cast<std::size_t>(ProvinceType::Craton))
                        cratonLand += CellArea(c);
                }
            }
        }
        const double share = cratonLand / seedLand;
        CHECK_MESSAGE((share > 0.05 && share < 0.40), "craton share of land, seed ", seed, ": ", share);
    }

    const auto landShare = [&](ProvinceType type) { return landByType[static_cast<std::size_t>(type)] / land; };
    CHECK(landShare(ProvinceType::Craton) > 0.10);
    CHECK(landShare(ProvinceType::Craton) < 0.30);
    CHECK(landShare(ProvinceType::Platform) > 0.25);
    CHECK(landShare(ProvinceType::CollisionOrogen) < 0.15);
    CHECK(landShare(ProvinceType::SedimentaryBasin) > 0.01);
    for (int type = 0; type < kProvinceTypeCount; ++type)
    {
        CHECK_MESSAGE(allByType[static_cast<std::size_t>(type)] > 0.0,
                      "province type never occurs: ", ProvinceTypeName(static_cast<ProvinceType>(type)));
    }
}

TEST_CASE("HeatFlowAndSedimentFollowCrustAgeAndSetting")
{
    const PlanetMap& map = SharedMap();
    double youngHeat = 0.0, youngArea = 0.0, oldHeat = 0.0, oldArea = 0.0;
    double youngSediment = 0.0, oldSediment = 0.0;
    double cratonHeat = 0.0, cratonArea = 0.0, riftHeat = 0.0, riftArea = 0.0;
    double marginSediment = 0.0, marginArea = 0.0, abyssalSediment = 0.0, abyssalArea = 0.0;
    double heatSum = 0.0, heatArea = 0.0;
    for (int la = 0; la < map.GetLatitudeBandCount(); ++la)
    {
        for (int lo = 0; lo < map.GetLongitudeSegmentCount(); ++lo)
        {
            const PlanetRegion& c = map.GetRegion(la, lo);
            const double area = CellArea(c);
            const ProvinceType type = map.FindProvince(c)->type;
            heatSum += c.heatFlowMwPerM2 * area;
            heatArea += area;
            if (c.continentalCrustFraction <= 0.5)
            {
                if (c.crustAgeMyr < 20.0)
                {
                    youngHeat += c.heatFlowMwPerM2 * area;
                    youngSediment += c.sedimentThicknessKm * area;
                    youngArea += area;
                }
                else if (c.crustAgeMyr > 100.0)
                {
                    oldHeat += c.heatFlowMwPerM2 * area;
                    oldSediment += c.sedimentThicknessKm * area;
                    oldArea += area;
                }
            }
            if (type == ProvinceType::Craton)
            {
                cratonHeat += c.heatFlowMwPerM2 * area;
                cratonArea += area;
            }
            if (type == ProvinceType::RiftSystem)
            {
                riftHeat += c.heatFlowMwPerM2 * area;
                riftArea += area;
            }
            if (type == ProvinceType::PassiveMargin)
            {
                marginSediment += c.sedimentThicknessKm * area;
                marginArea += area;
            }
            if (type == ProvinceType::AbyssalPlain)
            {
                abyssalSediment += c.sedimentThicknessKm * area;
                abyssalArea += area;
            }
        }
    }
    REQUIRE(youngArea > 0.0);
    REQUIRE(oldArea > 0.0);
    REQUIRE(cratonArea > 0.0);
    REQUIRE(riftArea > 0.0);
    REQUIRE(marginArea > 0.0);
    REQUIRE(abyssalArea > 0.0);

    CHECK((youngHeat / youngArea > 2.0 * (oldHeat / oldArea)));       // seafloor cools with age
    CHECK((oldSediment / oldArea > youngSediment / youngArea));       // and collects sediment
    CHECK((riftHeat / riftArea > cratonHeat / cratonArea));           // rifts are hot, cratons cold
    CHECK((marginSediment / marginArea > abyssalSediment / abyssalArea));
    // Earth's global mean surface heat flow is about 87 mW/m2.
    CHECK((heatSum / heatArea > 60.0));
    CHECK((heatSum / heatArea < 120.0));
}

TEST_CASE("ProvinceAgesAndErosionAreConsistent")
{
    const PlanetMap& map = SharedMap();
    const double planetAgeMyr = map.GetClimateInputs().planetAgeYears / 1.0e6;
    std::set<std::string_view> names;
    for (int type = 0; type < kProvinceTypeCount; ++type)
    {
        const std::string_view name = ProvinceTypeName(static_cast<ProvinceType>(type));
        CHECK(!name.empty());
        CHECK(name != "unknown");
        CHECK(names.insert(name).second);
    }

    CHECK(ClassifyErosionDepth(0.0) == ErosionLevel::Surface);
    CHECK(ClassifyErosionDepth(2.0) == ErosionLevel::Shallow);
    CHECK(ClassifyErosionDepth(8.0) == ErosionLevel::Intermediate);
    CHECK(ClassifyErosionDepth(20.0) == ErosionLevel::Deep);

    for (const Province& province : map.GetProvinces())
    {
        Require(province.eventAgeMyr >= 0.0 && province.eventAgeMyr <= planetAgeMyr,
                "Province event age is outside the planet's history.");
        Require(province.erosionDepthKm >= 0.0 && province.erosionDepthKm <= 30.0,
                "Province erosion depth is out of range.");
        Require(province.erosionLevel == ClassifyErosionDepth(province.erosionDepthKm),
                "Erosion level does not match the erosion depth.");
        if (province.type == ProvinceType::Craton)
        {
            Require(province.eventAgeMyr > planetAgeMyr * 0.5, "A craton is younger than half the planet.");
        }
        if (province.type == ProvinceType::CollisionOrogen || province.type == ProvinceType::RiftSystem ||
            province.type == ProvinceType::ContinentalArc)
        {
            Require(province.eventAgeMyr < planetAgeMyr * 0.1, "An active belt has an ancient main event.");
        }
        if (province.type == ProvinceType::AncientOrogen)
        {
            Require(province.eventAgeMyr > planetAgeMyr * 0.05, "An ancient orogen has a recent main event.");
        }
    }
}
