#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "planet/PlanetMap.h"
#include "planet/geology/PlanetEpochs.h"
#include "planet/geology/Province.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>
#include <string>
#include <utility>
#include <vector>

#define Require(condition, message)     do { if (!(condition)) { FAIL_CHECK(message); } } while (0)

namespace
{
    using namespace Infinitesia;

    constexpr double kEarthRadiusKm = 6371.0;

    double CellArea(const PlanetRegion& region)
    {
        return std::cos(region.latitudeDegrees * std::numbers::pi / 180.0);
    }

    // The default 0.5 degree planet, built once and shared.
    const PlanetMap& SharedMap()
    {
        static const PlanetMap map;
        return map;
    }

    // A coarser planet per seed, for statistics over several worlds.
    PlanetMap CoarseMap(std::uint32_t seed)
    {
        PlanetParameters parameters;
        parameters.latitudeBands = 180;
        parameters.longitudeSegments = 360;
        parameters.seed = seed;
        return PlanetMap(parameters);
    }

    bool IsOceanicType(ProvinceType type)
    {
        return type == ProvinceType::OceanicRidge || type == ProvinceType::AbyssalPlain ||
               type == ProvinceType::Trench || type == ProvinceType::IslandArc;
    }
}

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

TEST_CASE("EpochTableCoversThePlanetsHistoryWithoutGaps")
{
    for (const double ageMyr : { 4540.0, 2270.0, 9000.0 })
    {
        const PlanetEpochTable table(ageMyr);
        CHECK(table.PlanetAgeMyr() == doctest::Approx(ageMyr));
        for (const EpochRank rank : { EpochRank::Eon, EpochRank::Era })
        {
            // Eras begin with the Eoarchean; the Hadean has no subdivisions.
            double expectedStart = rank == EpochRank::Eon ? ageMyr : ageMyr * 4000.0 / 4540.0;
            int count = 0;
            for (const Epoch& epoch : table.Epochs())
            {
                if (epoch.rank != rank)
                    continue;
                CHECK(epoch.startMyr == doctest::Approx(expectedStart));
                CHECK(epoch.endMyr < epoch.startMyr);
                expectedStart = epoch.endMyr;
                ++count;
            }
            // Eons and eras tile the whole history from the planet's formation to now.
            if (rank == EpochRank::Eon)
                CHECK(count == 4);
            CHECK(expectedStart == doctest::Approx(0.0).epsilon(1e-9));
        }
    }

    const PlanetEpochTable earth(4540.0);
    REQUIRE(earth.EpochAt(0.0, EpochRank::Period) != nullptr);
    CHECK(earth.EpochAt(0.0, EpochRank::Period)->name == "Quaternary");
    CHECK(earth.EpochAt(100.0, EpochRank::Period)->name == "Cretaceous");
    CHECK(earth.EpochAt(66.0, EpochRank::Era)->name == "Cenozoic");   // a boundary belongs to the younger unit
    CHECK(earth.EpochAt(300.0, EpochRank::Era)->name == "Paleozoic");
    CHECK(earth.EpochAt(3000.0, EpochRank::Eon)->name == "Archean");
    CHECK(earth.EpochAt(3000.0, EpochRank::Period) == nullptr);       // no periods before the Siderian
    CHECK(earth.EpochAt(5000.0, EpochRank::Eon) == nullptr);
    CHECK(earth.EpochAt(-1.0, EpochRank::Eon) == nullptr);

    // The same sequence fits a planet half as old.
    const PlanetEpochTable young(2270.0);
    CHECK(young.EpochAt(50.0, EpochRank::Period)->name == "Cretaceous");
    CHECK(young.EpochAt(1500.0, EpochRank::Eon)->name == "Archean");
}

TEST_CASE("BiosphereStatesGateGeologicalContent")
{
    const PlanetEpochTable earth(4540.0);

    const BiosphereState archean = earth.BiosphereAt(3500.0);
    CHECK_FALSE(archean.freeOxygen);
    CHECK_FALSE(archean.landPlants);

    const BiosphereState bifEra = earth.BiosphereAt(2000.0);
    CHECK(bifEra.freeOxygen);
    CHECK(bifEra.bandedIronFormations);
    CHECK_FALSE(bifEra.landPlants);
    CHECK_FALSE(earth.BiosphereAt(2600.0).freeOxygen);
    CHECK(earth.BiosphereAt(2600.0).bandedIronFormations);
    CHECK_FALSE(earth.BiosphereAt(1500.0).bandedIronFormations);

    const BiosphereState carboniferous = earth.BiosphereAt(320.0);
    CHECK(carboniferous.landPlants);
    CHECK(carboniferous.organicRichMarineSediments);
    CHECK_FALSE(carboniferous.calcareousPlankton);

    const BiosphereState cretaceous = earth.BiosphereAt(100.0);
    CHECK(cretaceous.calcareousPlankton);
    CHECK_FALSE(cretaceous.bandedIronFormations);

    // Each milestone, once reached, stays reached.
    bool oxygen = false, plants = false, plankton = false, organic = false;
    for (double age = 4540.0; age >= 0.0; age -= 5.0)
    {
        const BiosphereState state = earth.BiosphereAt(age);
        CHECK((!oxygen || state.freeOxygen));
        CHECK((!plants || state.landPlants));
        CHECK((!plankton || state.calcareousPlankton));
        CHECK((!organic || state.organicRichMarineSediments));
        oxygen = oxygen || state.freeOxygen;
        plants = plants || state.landPlants;
        plankton = plankton || state.calcareousPlankton;
        organic = organic || state.organicRichMarineSediments;
    }

    // Milestones scale with the planet's age; a lifeless scenario has none of them.
    const PlanetEpochTable old(9080.0);
    CHECK(old.BiosphereAt(4900.0).freeOxygen == false);
    CHECK(old.BiosphereAt(4799.0).freeOxygen == true);
    CHECK(old.BiosphereAt(4000.0).freeOxygen == true);
    const PlanetEpochTable lifeless(4540.0, false);
    const BiosphereState none = lifeless.BiosphereAt(0.0);
    CHECK_FALSE(none.freeOxygen);
    CHECK_FALSE(none.bandedIronFormations);
    CHECK_FALSE(none.landPlants);
    CHECK_FALSE(none.organicRichMarineSediments);
    CHECK_FALSE(none.calcareousPlankton);
    CHECK(lifeless.EpochAt(100.0, EpochRank::Period) != nullptr);
}

TEST_CASE("PlanetMapExposesItsEpochTable")
{
    const PlanetMap& map = SharedMap();
    CHECK(map.GetEpochs().PlanetAgeMyr() == doctest::Approx(map.GetClimateInputs().planetAgeYears / 1.0e6));
    CHECK(map.GetEpochs().HasBiosphere());
    CHECK(kPlanetGeologyDataVersion >= 1);
}
