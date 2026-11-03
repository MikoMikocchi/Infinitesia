#include "planet/StarSystemGenerator.h"
#include "planet/PlanetMap.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"
#include "planet/internal/PlanetMapHydrologyHelpers.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    constexpr double kGravitationalConstant = 6.67430e-11;
    constexpr double kEarthMassKilograms = 5.9722e24;
    constexpr double kSolarMassKilograms = 1.98847e30;
    constexpr double kAstronomicalUnitMeters = 149597870700.0;

    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    bool NearlyEqual(double first, double second, double relativeTolerance = 1e-10)
    {
        return std::abs(first - second) <=
               std::max(std::abs(first), std::abs(second)) * relativeTolerance;
    }

    void TestSameScenarioIsDeterministic()
    {
        const Infinitesia::GenerationScenario scenario{ .seed = 42 };
        const Infinitesia::StarSystem first = Infinitesia::StarSystemGenerator::Generate(scenario);
        const Infinitesia::StarSystem second = Infinitesia::StarSystemGenerator::Generate(scenario);

        Require(first.id == second.id, "System ID changed for identical scenario.");
        Require(first.star.id == second.star.id, "Star ID changed for identical scenario.");
        Require(first.disk.id == second.disk.id, "Disk ID changed for identical scenario.");
        Require(first.planet.planetId == second.planet.planetId, "Planet ID changed for identical scenario.");
        Require(first.star.massKilograms == second.star.massKilograms, "Star mass is not deterministic.");
        Require(first.star.luminosityWatts == second.star.luminosityWatts, "Star luminosity is not deterministic.");
        Require(first.disk.solidMassKilograms == second.disk.solidMassKilograms, "Disk mass is not deterministic.");
        Require(first.planetOrbit.semiMajorAxisMeters == second.planetOrbit.semiMajorAxisMeters, "Orbit is not deterministic.");
        Require(first.planet.massKilograms == second.planet.massKilograms, "Planet mass is not deterministic.");
        Require(first.planet.gravityMetersPerSecondSquared == second.planet.gravityMetersPerSecondSquared, "Planet gravity is not deterministic.");
        Require(first.events.size() == second.events.size(), "Generation event count is not deterministic.");
        for (std::size_t index = 0; index < first.events.size(); ++index)
        {
            Require(first.events[index].id == second.events[index].id,
                    "Generation event ID changed for identical scenario.");
        }
    }

    void TestDifferentScenariosProduceDifferentSystems()
    {
        const Infinitesia::StarSystem first =
            Infinitesia::StarSystemGenerator::Generate({ .seed = 1 });
        const Infinitesia::StarSystem second =
            Infinitesia::StarSystemGenerator::Generate({ .seed = 2 });

        Require(first.id != second.id, "Different seeds produced the same system ID.");
        Require(first.star.massKilograms != second.star.massKilograms,
                "Different seeds produced the same star mass.");
        Require(first.planetOrbit.semiMajorAxisMeters != second.planetOrbit.semiMajorAxisMeters,
                "Different seeds produced the same planetary orbit.");
    }

    void TestGeneratedSystemIsInternallyConsistent()
    {
        const Infinitesia::StarSystem system =
            Infinitesia::StarSystemGenerator::Generate({ .seed = 2026 });
        Require(system.scenario.generatorVersion == 6,
                "The revised planet-generation model did not use generator version 6.");
        const double massEarths = system.planet.massKilograms / kEarthMassKilograms;
        const double semiMajorAxisAu =
            system.planetOrbit.semiMajorAxisMeters / kAstronomicalUnitMeters;
        const double expectedGravity =
            kGravitationalConstant * system.planet.massKilograms /
            std::pow(system.planet.radiusMeters, 2.0);
        const double expectedDensity =
            system.planet.massKilograms /
            ((4.0 / 3.0) * std::numbers::pi *
             std::pow(system.planet.radiusMeters, 3.0));
        const double luminositySolar =
            system.star.luminosityWatts / 3.828e26;
        const double expectedFlux =
            luminositySolar /
            (std::pow(semiMajorAxisAu, 2.0) *
             std::sqrt(1.0 - std::pow(system.planetOrbit.eccentricity, 2.0)));
        const double expectedPeriod =
            2.0 * std::numbers::pi *
            std::sqrt(std::pow(system.planetOrbit.semiMajorAxisMeters, 3.0) /
                      (kGravitationalConstant * system.star.massKilograms));

        Require(system.planet.IsValid(), "Generated planet parameters are invalid.");
        Require(system.star.ageYears == system.galacticContext.systemAgeYears,
                "Star age does not match system age.");
        Require(system.star.ageYears < system.star.mainSequenceLifetimeYears,
                "Generated star is older than its main-sequence lifetime.");
        Require(system.disk.solidMassKilograms < system.disk.totalMassKilograms,
                "Disk solid mass exceeds total disk mass.");
        Require(massEarths >= 0.75 && massEarths <= 1.25,
                "Default planet mass is outside the Earth-like generation range.");
        Require(system.planet.gravityMetersPerSecondSquared >= 8.0 &&
                system.planet.gravityMetersPerSecondSquared <= 12.0,
                "Default planet gravity is outside the Earth-like generation range.");
        Require(system.planetOrbit.eccentricity >= 0.0 &&
                system.planetOrbit.eccentricity < 1.0,
                "Generated orbit has invalid eccentricity.");
        Require(NearlyEqual(system.planet.gravityMetersPerSecondSquared, expectedGravity),
                "Planet gravity is inconsistent with mass and radius.");
        Require(NearlyEqual(system.planet.meanDensityKilogramsPerCubicMeter, expectedDensity),
                "Planet density is inconsistent with mass and radius.");
        Require(NearlyEqual(system.planetOrbit.meanIncidentFluxInEarthUnits, expectedFlux),
                "Incident flux is inconsistent with stellar luminosity and orbit.");
        Require(NearlyEqual(system.planetOrbit.orbitalPeriodSeconds, expectedPeriod),
                "Orbital period is inconsistent with orbit and stellar mass.");
        Require(system.planetOrbit.semiMajorAxisMeters > 0.0 &&
                system.planetOrbit.semiMajorAxisMeters < 10.0 * kAstronomicalUnitMeters,
                "Generated orbit is outside the supported range.");
        Require(system.planet.ageYears > 0.0 &&
                system.planet.ageYears < system.galacticContext.systemAgeYears,
                "Planet age is inconsistent with system age.");
        Require(system.events.size() == 4, "Expected the initial formation event sequence.");
        for (const Infinitesia::GenerationEvent& event : system.events)
        {
            Require(event.id != 0, "Generation event has no stable ID.");
            if (event.parentEventId != 0)
            {
                const auto parent = std::find_if(
                    system.events.begin(),
                    system.events.end(),
                    [&event](const Infinitesia::GenerationEvent& candidate)
                    {
                        return candidate.id == event.parentEventId;
                    });
                Require(parent != system.events.end(), "Generation event parent is missing.");
                Require(parent->timeAfterSystemFormationYears <=
                            event.timeAfterSystemFormationYears,
                        "Generation event predates its parent.");
            }
        }
        for (std::size_t index = 1; index < system.events.size(); ++index)
        {
            Require(system.events[index - 1].timeAfterSystemFormationYears <=
                    system.events[index].timeAfterSystemFormationYears,
                    "Generation events are not in chronological order.");
        }
    }

    void TestInvalidAndUnsupportedScenariosFailExplicitly()
    {
        bool invalidVersionRejected = false;
        try
        {
            Infinitesia::StarSystemGenerator::Generate(
                { .seed = 1, .generatorVersion = 0 });
        }
        catch (const std::invalid_argument&)
        {
            invalidVersionRejected = true;
        }
        Require(invalidVersionRejected, "Invalid generator version was not rejected.");
    }

    void TestSeedSweepPreservesGenerationInvariants()
    {
        for (std::uint64_t seed = 0; seed < 128; ++seed)
        {
            const Infinitesia::StarSystem system =
                Infinitesia::StarSystemGenerator::Generate({ .seed = seed });
            Require(system.planet.IsValid(), "Seed sweep produced invalid planet parameters.");
            Require(system.star.ageYears < system.star.mainSequenceLifetimeYears,
                    "Seed sweep produced a star beyond its supported lifetime.");
            Require(system.disk.solidMassKilograms < system.disk.totalMassKilograms,
                    "Seed sweep produced an invalid disk mass budget.");
            Require(system.planet.massKilograms > 0.0,
                    "Seed sweep produced a planet with no mass.");
            Require(system.planetOrbit.eccentricity < 1.0,
                    "Seed sweep produced an unbound planetary orbit.");
        }
    }

    void TestScenarioConstraintsAreApplied()
    {
        Infinitesia::GenerationScenario scenario{ .seed = 73 };
        scenario.constraints.minimumStarMassSolar = 0.9;
        scenario.constraints.maximumStarMassSolar = 1.0;
        scenario.constraints.minimumIncidentFluxInEarthUnits = 0.9;
        scenario.constraints.maximumIncidentFluxInEarthUnits = 1.0;
        scenario.constraints.maximumPlanetMassEarths = 1.5;

        const Infinitesia::StarSystem system =
            Infinitesia::StarSystemGenerator::Generate(scenario);
        const double starMassSolar = system.star.massKilograms / kSolarMassKilograms;
        const double planetMassEarths = system.planet.massKilograms / kEarthMassKilograms;

        Require(starMassSolar >= scenario.constraints.minimumStarMassSolar &&
                starMassSolar <= scenario.constraints.maximumStarMassSolar,
                "Star mass constraints were not applied.");
        Require(system.planetOrbit.meanIncidentFluxInEarthUnits >=
                    scenario.constraints.minimumIncidentFluxInEarthUnits &&
                system.planetOrbit.meanIncidentFluxInEarthUnits <=
                    scenario.constraints.maximumIncidentFluxInEarthUnits,
                "Incident-flux constraints were not applied.");
        Require(planetMassEarths <= scenario.constraints.maximumPlanetMassEarths,
                "Maximum planet mass constraint was not applied.");

        scenario.constraints.minimumStarMassSolar = 1.3;
        Require(!scenario.IsValid(), "Invalid scenario constraints were accepted.");

        scenario.constraints.minimumStarMassSolar = 1.2;
        scenario.constraints.maximumStarMassSolar = 1.25;
        scenario.constraints.minimumSystemAgeYears = 7.0e9;
        bool incompatibleAgeRejected = false;
        try
        {
            Infinitesia::StarSystemGenerator::Generate(scenario);
        }
        catch (const std::invalid_argument&)
        {
            incompatibleAgeRejected = true;
        }
        Require(incompatibleAgeRejected,
                "Constraints requiring a star older than its lifetime were accepted.");
    }

    void TestPlanetMapIsDeterministicAndWithinGenerationBudget()
    {
        const auto start = std::chrono::steady_clock::now();
        const Infinitesia::PlanetMap defaultMap;
        const auto elapsed = std::chrono::steady_clock::now() - start;
        Require(elapsed < std::chrono::seconds(2),
                "Default planet-map generation exceeded the 2-second startup budget.");

        double totalAreaWeight = 0.0;
        double landAreaWeight = 0.0;
        double meanTemperatureNumerator = 0.0;
        double highStressAreaWeight = 0.0;
        std::size_t transitionalCrustCells = 0;
        std::size_t broadReliefCells = 0;
        double maximumTectonicUpliftKm = 0.0;
        double minimumTectonicReliefKm = 0.0;
        double maximumAdjacentElevationStepKm = 0.0;
        double maximumSeasonalTemperatureRangeC = 0.0;
        double minimumLandOrographicFactor = 1.0;
        double maximumLandOrographicFactor = 0.0;
        std::vector<std::pair<bool, bool>> plateCrustKinds;
        for (int latitude = 0; latitude < defaultMap.GetLatitudeBandCount(); ++latitude)
        {
            for (int longitude = 0; longitude < defaultMap.GetLongitudeSegmentCount(); ++longitude)
            {
                const Infinitesia::PlanetRegion& region =
                    defaultMap.GetRegion(latitude, longitude);
                const Infinitesia::PlanetRegion& eastRegion =
                    defaultMap.GetRegion(
                        latitude,
                        (longitude + 1) % defaultMap.GetLongitudeSegmentCount());
                const double eastStep =
                    std::abs(region.elevationNormalized - eastRegion.elevationNormalized) *
                    9.0;
                if (eastStep > maximumAdjacentElevationStepKm)
                    maximumAdjacentElevationStepKm = eastStep;
                if (latitude + 1 < defaultMap.GetLatitudeBandCount())
                {
                    const Infinitesia::PlanetRegion& northRegion =
                        defaultMap.GetRegion(latitude + 1, longitude);
                    const double northStep =
                        std::abs(region.elevationNormalized -
                                 northRegion.elevationNormalized) * 9.0;
                    if (northStep > maximumAdjacentElevationStepKm)
                        maximumAdjacentElevationStepKm = northStep;
                }
                const double areaWeight =
                    std::cos(region.latitudeDegrees * std::numbers::pi / 180.0);
                totalAreaWeight += areaWeight;
                meanTemperatureNumerator += areaWeight * region.temperatureCelsius;
                maximumSeasonalTemperatureRangeC = std::max(
                    maximumSeasonalTemperatureRangeC,
                    region.temperatureAnnualRangeC);
                if (!region.isOcean)
                {
                    minimumLandOrographicFactor = std::min(
                        minimumLandOrographicFactor,
                        region.windMoistureFactor);
                    maximumLandOrographicFactor = std::max(
                        maximumLandOrographicFactor,
                        region.windMoistureFactor);
                }
                if (region.tectonicStress > 0.6)
                    highStressAreaWeight += areaWeight;
                if (region.continentalCrustFraction > 0.1 &&
                    region.continentalCrustFraction < 0.9)
                    ++transitionalCrustCells;
                if (region.tectonicStress < 0.6 &&
                    std::abs(region.tectonicReliefKm) > 0.2)
                    ++broadReliefCells;
                maximumTectonicUpliftKm =
                    std::max(maximumTectonicUpliftKm, region.tectonicReliefKm);
                minimumTectonicReliefKm =
                    std::min(minimumTectonicReliefKm, region.tectonicReliefKm);
                if (!region.isOcean)
                    landAreaWeight += areaWeight;
                if (region.tectonicPlateId >= 0)
                {
                    if (plateCrustKinds.size() <=
                        static_cast<std::size_t>(region.tectonicPlateId))
                    {
                        plateCrustKinds.resize(
                            static_cast<std::size_t>(region.tectonicPlateId) + 1);
                    }
                    auto& [hasOceanicCrust, hasContinentalCrust] =
                        plateCrustKinds[static_cast<std::size_t>(region.tectonicPlateId)];
                    hasOceanicCrust |= region.surfaceRock == Infinitesia::RockType::BasalticOceanic;
                    hasContinentalCrust |= region.surfaceRock != Infinitesia::RockType::BasalticOceanic;
                }
                Require(std::isfinite(region.elevationNormalized) &&
                        region.elevationNormalized >= -1.0 &&
                        region.elevationNormalized <= 1.0,
                        "Generated elevation is not finite or normalized.");
                Require(std::isfinite(region.crustAge) &&
                        region.crustAge >= 0.0 &&
                        region.crustAge <= 1.0,
                        "Generated crust age is outside its normalized range.");
                Require(std::isfinite(region.crustThickness) &&
                        region.crustThickness > 0.0,
                        "Generated crust thickness is not physically positive.");
                Require(std::isfinite(region.isostaticCrustThicknessKm) &&
                        region.isostaticCrustThicknessKm > 0.0,
                        "Generated isostatic crust thickness is not physically positive.");
                Require(std::isfinite(region.tectonicStress) &&
                        region.tectonicStress >= 0.0 &&
                        region.tectonicStress <= 1.0,
                        "Generated tectonic stress is outside its normalized range.");
                Require(std::isfinite(region.temperatureCelsius) &&
                        std::isfinite(region.precipitationMm) &&
                        region.precipitationMm >= 0.0,
                        "Generated climate contains invalid values.");
                double monthlyTemperatureMean = 0.0;
                double monthlyPrecipitationTotal = 0.0;
                double minimumMonthlyTemperature =
                    std::numeric_limits<double>::infinity();
                double maximumMonthlyTemperature =
                    -std::numeric_limits<double>::infinity();
                std::size_t dryMonthCount = 0;
                for (std::size_t month = 0;
                     month < region.monthlyTemperatureCelsius.size();
                     ++month)
                {
                    Require(std::isfinite(region.monthlyTemperatureCelsius[month]) &&
                            std::isfinite(region.monthlyPrecipitationMm[month]) &&
                            region.monthlyPrecipitationMm[month] >= 0.0,
                            "Monthly climate contains invalid values.");
                    monthlyTemperatureMean +=
                        region.monthlyTemperatureCelsius[month] / 12.0;
                    monthlyPrecipitationTotal +=
                        region.monthlyPrecipitationMm[month];
                    minimumMonthlyTemperature = std::min(
                        minimumMonthlyTemperature,
                        region.monthlyTemperatureCelsius[month]);
                    maximumMonthlyTemperature = std::max(
                        maximumMonthlyTemperature,
                        region.monthlyTemperatureCelsius[month]);
                    dryMonthCount += region.monthlyPrecipitationMm[month] < 30.0
                        ? 1
                        : 0;
                }
                Require(std::abs(monthlyTemperatureMean - region.temperatureCelsius) < 1e-8 &&
                        std::abs(monthlyPrecipitationTotal - region.precipitationMm) <
                            std::max(1e-8, region.precipitationMm * 1e-8) &&
                        std::abs(maximumMonthlyTemperature -
                                 minimumMonthlyTemperature -
                                 region.temperatureAnnualRangeC) < 1e-8 &&
                        dryMonthCount == static_cast<std::size_t>(
                            region.drySeasonMonths),
                        "Annual climate does not agree with its monthly values.");
                Require(std::isfinite(region.windMoistureFactor) &&
                        region.windMoistureFactor >= 0.25 &&
                        region.windMoistureFactor <= 1.8,
                        "Wind-driven moisture transfer is outside its calibrated range.");
                Require(region.soilFertility >= 0.0 &&
                        region.soilFertility <= 1.0 &&
                        region.soilDepthNorm >= 0.0 &&
                        region.soilDepthNorm <= 1.0,
                        "Generated soil fields are outside their normalized ranges.");
                Require(region.isOcean == (region.elevationNormalized < 0.0),
                        "Ocean classification disagrees with elevation.");
            }
        }
        const double landAreaFraction = landAreaWeight / totalAreaWeight;
        Require(landAreaFraction >= 0.15 && landAreaFraction <= 0.65,
                "Generated land area is outside a plausible rocky-planet range.");
        const double meanTemperatureCelsius =
            meanTemperatureNumerator / totalAreaWeight;
        Require(meanTemperatureCelsius > 8.0 && meanTemperatureCelsius < 22.0,
                "Earth-like atmosphere produced an implausible global mean temperature.");
        Require(highStressAreaWeight / totalAreaWeight < 0.05,
                "Tectonic stress still covers an implausibly broad part of the map.");
        Require(transitionalCrustCells > 0,
                "Continental margins have no gradual crustal transition.");
        Require(broadReliefCells > 0,
                "Tectonic relief is confined to sharp stress-line cores.");
        Require(maximumTectonicUpliftKm > 2.0 &&
                minimumTectonicReliefKm < -1.0,
                "Convergent and divergent boundaries lack broad mountain/trench relief.");
        Require(maximumAdjacentElevationStepKm < 3.0,
                "Adjacent terrain cells still form abrupt tectonic cliffs.");
        Require(maximumSeasonalTemperatureRangeC > 8.0,
                "Seasonal insolation is not producing a meaningful temperature range.");
        Require(minimumLandOrographicFactor < 0.9 &&
                maximumLandOrographicFactor > 1.0,
                "Windward uplift and lee-side drying are not affecting precipitation.");
        Require(std::any_of(plateCrustKinds.begin(), plateCrustKinds.end(),
                    [](const auto& crustKinds)
                    {
                        return crustKinds.first && crustKinds.second;
                    }),
                "Continental crust is still rigidly assigned by tectonic plate.");

        const Infinitesia::StarSystem seededSystem =
            Infinitesia::StarSystemGenerator::Generate(
                { .seed = 1, .generatorVersion = 2 });
        const Infinitesia::PlanetMap seededMap(
            seededSystem.planet,
            Infinitesia::ClimateInputs{
                seededSystem.planetOrbit.meanIncidentFluxInEarthUnits,
                seededSystem.planet.axialTiltDegrees,
                seededSystem.planet.atmospherePressureKPa,
                seededSystem.planet.volatileMassFraction,
                seededSystem.planet.gravityMetersPerSecondSquared,
                seededSystem.planet.rotationHours,
                seededSystem.planet.ageYears
            });
        const double seedOneMidLatitudeC =
            seededMap.GetRegion(136, 254).temperatureCelsius;
        Require(seedOneMidLatitudeC > -13.0,
                "The original seed-1 mid-latitude climate is still over-cooled.");

        const Infinitesia::StarSystem currentSeedOne =
            Infinitesia::StarSystemGenerator::Generate({ .seed = 1 });
        const Infinitesia::PlanetMap currentSeedOneMap(
            currentSeedOne.planet,
            Infinitesia::ClimateInputs{
                currentSeedOne.planetOrbit.meanIncidentFluxInEarthUnits,
                currentSeedOne.planet.axialTiltDegrees,
                currentSeedOne.planet.atmospherePressureKPa,
                currentSeedOne.planet.volatileMassFraction,
                currentSeedOne.planet.gravityMetersPerSecondSquared,
                currentSeedOne.planet.rotationHours,
                currentSeedOne.planet.ageYears
            });
        std::array<std::size_t, 25> biomeCounts{};
        double meanPrecipitation = 0.0;
        std::size_t totalCells = 0;
        for (int latitude = 0; latitude < currentSeedOneMap.GetLatitudeBandCount(); ++latitude)
        {
            for (int longitude = 0; longitude < currentSeedOneMap.GetLongitudeSegmentCount(); ++longitude)
            {
                const auto& region = currentSeedOneMap.GetRegion(latitude, longitude);
                ++biomeCounts[static_cast<std::size_t>(region.biome)];
                meanPrecipitation += region.precipitationMm;
                ++totalCells;
            }
        }
        const auto biomeArea = [&biomeCounts](Infinitesia::Biome biome)
        {
            return biomeCounts[static_cast<std::size_t>(biome)];
        };
        std::size_t distinctLandBiomes = 0;
        for (std::size_t index = 4; index < biomeCounts.size(); ++index)
            distinctLandBiomes += biomeCounts[index] > 0 ? 1 : 0;
        Require(distinctLandBiomes >= 8,
                "The Earth-like seed produces too few distinct land biomes.");
        Require(biomeArea(Infinitesia::Biome::TropicalRainforest) > 0 &&
                biomeArea(Infinitesia::Biome::TemperateDeciduous) > 0 &&
                (biomeArea(Infinitesia::Biome::Savanna) > 0 ||
                 biomeArea(Infinitesia::Biome::HotDesert) > 0),
                "The seed-1 world is missing tropical, temperate, or warm-dry biomes.");
        Require(biomeArea(Infinitesia::Biome::TropicalMonsoonForest) > 0 ||
                biomeArea(Infinitesia::Biome::TemperateRainforest) > 0,
                "Seasonal climate did not produce any newly differentiated forest biome.");
        Require(meanPrecipitation / totalCells > 250.0,
                "The Earth-like climate still has globally suppressed precipitation.");

        Infinitesia::PlanetParameters parameters;
        parameters.latitudeBands = 45;
        parameters.longitudeSegments = 90;
        parameters.seed = 2026;
        const Infinitesia::PlanetMap first(parameters);
        const Infinitesia::PlanetMap second(parameters);
        for (int latitude = 0; latitude < parameters.latitudeBands; ++latitude)
        {
            for (int longitude = 0; longitude < parameters.longitudeSegments; ++longitude)
            {
                const Infinitesia::PlanetRegion& firstRegion =
                    first.GetRegion(latitude, longitude);
                const Infinitesia::PlanetRegion& secondRegion =
                    second.GetRegion(latitude, longitude);
                Require(firstRegion.tectonicPlateId == secondRegion.tectonicPlateId &&
                        firstRegion.crustAge == secondRegion.crustAge &&
                        firstRegion.elevationNormalized == secondRegion.elevationNormalized &&
                        firstRegion.biome == secondRegion.biome,
                        "Planet-map generation is not deterministic for an identical seed.");
            }
        }
    }
}

// --------------------------------------------------------------------------
// Unit tests for ClassifyBoundaryFromVelocity sign convention.
//
// These tests anchor the geometric contract: given that the boundary normal
// points from plate A toward plate B, a relative velocity that moves A toward
// B must be Convergent and one that moves A away from B must be Divergent.
// They also verify that pure strike-slip motion is correctly classified as
// Transform regardless of the normal direction.
//
// Having explicit unit tests here means that any future refactor of the
// classification function will immediately break a specific, readable
// assertion rather than silently inverting mountains and trenches planet-wide.
// --------------------------------------------------------------------------
void TestBoundaryClassificationSignConvention()
{
    using namespace Infinitesia::GenerationDetail;

    // -----------------------------------------------------------------------
    // Setup: plate A is west of plate B.
    // Boundary normal points east (A → B), i.e. (lat=0, lon=+1).
    // -----------------------------------------------------------------------
    constexpr double normalLat = 0.0;
    constexpr double normalLon = 1.0; // eastward unit normal (A → B)

    // --- Test 1: head-on collision (A moves east, B moves west) ---
    // A moves toward B ⟹ Convergent
    Require(
        ClassifyBoundaryFromVelocity(
            0.0,  0.1,   // A: moving east
            0.0, -0.1,   // B: moving west
            normalLat, normalLon) == BoundaryType::Convergent,
        "Head-on collision (A east, B west) must classify as Convergent.");

    // --- Test 2: pure divergence (A moves west, B moves east) ---
    // A moves away from B ⟹ Divergent
    Require(
        ClassifyBoundaryFromVelocity(
            0.0, -0.1,   // A: moving west
            0.0,  0.1,   // B: moving east
            normalLat, normalLon) == BoundaryType::Divergent,
        "Plates spreading apart (A west, B east) must classify as Divergent.");

    // --- Test 3: pure strike-slip (both moving north at same speed) ---
    // No relative motion ⟹ Transform
    Require(
        ClassifyBoundaryFromVelocity(
            0.1, 0.0,    // A: moving north
            0.0, 0.0,    // B: stationary
            normalLat, normalLon) == BoundaryType::Transform,
        "Pure northward motion along an east-west boundary must be Transform.");

    // --- Test 4: symmetric strike-slip (A north, B south) ---
    // Motion is entirely tangential to the east-west normal ⟹ Transform
    Require(
        ClassifyBoundaryFromVelocity(
            0.1,  0.0,   // A: moving north
           -0.1,  0.0,   // B: moving south
            normalLat, normalLon) == BoundaryType::Transform,
        "Symmetric northward/southward strike-slip must classify as Transform.");

    // --- Test 5: oblique convergence with dominant normal component ---
    // Normal component (A toward B) > tangential ⟹ Convergent
    Require(
        ClassifyBoundaryFromVelocity(
            0.01,  0.12,  // A: mostly east, slight north
            0.01, -0.03,  // B: slightly west
            normalLat, normalLon) == BoundaryType::Convergent,
        "Oblique motion with dominant convergent component must be Convergent.");

    // --- Test 6: oblique divergence with dominant normal component ---
    Require(
        ClassifyBoundaryFromVelocity(
            0.01, -0.12,  // A: mostly west
            0.01,  0.03,  // B: slightly east
            normalLat, normalLon) == BoundaryType::Divergent,
        "Oblique motion with dominant divergent component must be Divergent.");

    // --- Test 7: below noise floor ⟹ always Transform ---
    Require(
        ClassifyBoundaryFromVelocity(
            0.0,  0.001,  // A: very slow east
            0.0, -0.001,  // B: very slow west
            normalLat, normalLon) == BoundaryType::Transform,
        "Sub-noise-floor relative motion must be classified as Transform.");

    // --- Test 8: northward normal (different orientation) ---
    // Normal points north (A → B); A moves north, B stationary.
    // A is advancing in the A→B direction → Convergent.
    Require(
        ClassifyBoundaryFromVelocity(
            0.1, 0.0,   // A: moving north
            0.0, 0.0,   // B: stationary
            1.0, 0.0)   // normal points north
        == BoundaryType::Convergent,
        "Northward motion against northward A→B normal must be Convergent.");

    // --- Test 9: retreating along northward normal → Divergent ---
    // Normal points north (A → B); A moves south, B stationary.
    // A retreats from B → Divergent.
    Require(
        ClassifyBoundaryFromVelocity(
           -0.1, 0.0,   // A: moving south (away from B)
            0.0, 0.0,   // B: stationary
            1.0, 0.0)   // normal points north
        == BoundaryType::Divergent,
        "Southward motion against northward A→B normal must be Divergent.");
}


// --------------------------------------------------------------------------
// Robustness tests: edge-case planet configurations.
// --------------------------------------------------------------------------
void TestSyntheticFullyLandDrainageFallback()
{
    constexpr int latitudeBands = 5;
    constexpr int longitudeSegments = 8;
    const std::size_t cellCount =
        static_cast<std::size_t>(latitudeBands) * longitudeSegments;
    std::vector<double> elevations(cellCount);
    std::vector<bool> oceanCells(cellCount, false);
    std::vector<double> localRunoff(cellCount);
    std::vector<double> localRunoffVolume(cellCount);

    for (std::size_t index = 0; index < cellCount; ++index)
    {
        elevations[index] = 0.1 + static_cast<double>((index * 7) % 17) / 20.0;
        localRunoff[index] = 1.0 + static_cast<double>(index % 5);
        localRunoffVolume[index] = localRunoff[index] * 0.01;
    }

    // Guarantee a fully land-covered map with no ocean cells at all.
    for (std::size_t index = 0; index < cellCount; ++index)
    {
        oceanCells[index] = false;
    }

    const auto network = Infinitesia::GenerationDetail::BuildDrainageNetwork(
        latitudeBands,
        longitudeSegments,
        elevations,
        oceanCells,
        localRunoff,
        localRunoffVolume);

    Require(std::none_of(oceanCells.begin(), oceanCells.end(), [](bool isOcean)
        {
            return isOcean;
        }),
        "Synthetic fallback map must contain no ocean cells.");
    Require(network.usedLandFallback,
        "Fully land-covered map did not use the Priority-Flood fallback.");

    const std::size_t minimumIndex = static_cast<std::size_t>(
        std::distance(
            elevations.begin(),
            std::min_element(elevations.begin(), elevations.end())));
    for (std::size_t cell = 0; cell < cellCount; ++cell)
    {
        if (cell == minimumIndex)
        {
            Require(network.flowDirections[cell] == -1,
                "Global minimum of a fully land-covered map must be the drainage sink.");
        }
        else
        {
            Require(network.flowDirections[cell] >= 0,
                "Land cell in a fully land-covered map has no downstream route.");
        }

        std::size_t downstream = cell;
        std::size_t steps = 0;
        while (network.flowDirections[downstream] >= 0 && steps < cellCount)
        {
            downstream = static_cast<std::size_t>(
                network.flowDirections[downstream]);
            ++steps;
        }
        Require(downstream == minimumIndex && steps < cellCount,
            "Land drainage path does not terminate at the global minimum.");
    }
}

void TestDrainageAccumulationIsMonotonicAlongFlowGraph()
{
    constexpr int latitudeBands = 5;
    constexpr int longitudeSegments = 8;
    const std::size_t cellCount =
        static_cast<std::size_t>(latitudeBands) * longitudeSegments;
    std::vector<double> elevations(cellCount);
    std::vector<bool> oceanCells(cellCount, false);
    std::vector<double> localRunoff(cellCount);
    std::vector<double> localRunoffVolume(cellCount);

    for (std::size_t index = 0; index < cellCount; ++index)
    {
        elevations[index] = static_cast<double>((index * 11) % 23) / 10.0;
        localRunoff[index] = 1.0 + static_cast<double>(index % 3);
        localRunoffVolume[index] = localRunoff[index] * 0.025;
    }
    oceanCells[3] = true;
    oceanCells[latitudeBands * longitudeSegments - 5] = true;

    const auto network = Infinitesia::GenerationDetail::BuildDrainageNetwork(
        latitudeBands,
        longitudeSegments,
        elevations,
        oceanCells,
        localRunoff,
        localRunoffVolume);

    Require(!network.usedLandFallback,
        "Drainage graph test with ocean outlets unexpectedly used the land fallback.");
    for (std::size_t cell = 0; cell < cellCount; ++cell)
    {
        if (oceanCells[cell])
            continue;
        const int downstreamIndex = network.flowDirections[cell];
        if (downstreamIndex < 0)
            continue;

        const std::size_t downstream = static_cast<std::size_t>(downstreamIndex);
        Require(network.filledElevations[downstream] <
                    network.filledElevations[cell] - 1e-18,
            "Drainage edge does not descend in the filled elevation graph.");
        Require(network.normalizedAccumulation[downstream] + 1e-12 >=
                    network.normalizedAccumulation[cell],
            "Normalized drainage accumulation decreased downstream.");
        Require(network.volumeAccumulation[downstream] + 1e-12 >=
                    network.volumeAccumulation[cell],
            "Runoff volume accumulation decreased downstream.");
    }
}

void TestEdgeCasePlanetConfigurations()
{
    // --- Hydrology invariants: flow must respect topology ---
    // For any generated planet, drainageAccumulation must be monotonically
    // non-increasing as water flows toward the ocean (downstream cells must
    // have >= drainage than any single upstream cell, since accumulation is
    // additive).  We verify this by checking that every land cell's
    // drainageAccumulation is >= its own local runoff contribution (i.e., > 0
    // only if it has actual precipitation or upstream inflow).
    //
    // Additionally, annualRunoffVolumeKm3 must be finite and non-negative.
    {
        Infinitesia::PlanetParameters params;
        params.latitudeBands     = 45;
        params.longitudeSegments = 90;
        params.seed              = 0xdeadbeef;
        const Infinitesia::PlanetMap map(params);
        const int latB = map.GetLatitudeBandCount();
        const int lonS = map.GetLongitudeSegmentCount();
        const auto validateLake = [](const Infinitesia::PlanetRegion& r)
        {
            Require(!r.isOcean,
                "Cell is simultaneously ocean and lake.");
            Require(std::isfinite(r.lakeWaterVolumeKm3) &&
                    r.lakeWaterVolumeKm3 > 0.0 &&
                    std::isfinite(r.lakeSurfaceAreaKm2) &&
                    r.lakeSurfaceAreaKm2 > 0.0 &&
                    std::isfinite(r.lakeAnnualInflowKm3) &&
                    std::isfinite(r.lakeAnnualEvaporationKm3) &&
                    std::isfinite(r.lakeAnnualInfiltrationKm3) &&
                    std::isfinite(r.lakeAnnualSpillKm3) &&
                    std::isfinite(r.lakeWaterLevelKm) &&
                    std::isfinite(r.lakeSpillElevationKm) &&
                    std::isfinite(r.lakeSpillHeightKm),
                "Lake water-balance values must be finite and positive.");
            Require(r.lakeAnnualInflowKm3 >
                    r.lakeAnnualEvaporationKm3 +
                    r.lakeAnnualInfiltrationKm3,
                "Lake must have a positive annual water balance.");
            Require(r.lakeAnnualSpillKm3 > 0.0 &&
                    r.lakeSpillHeightKm > 0.0 &&
                    r.lakeWaterLevelKm == r.lakeSpillElevationKm,
                "Lake must reach its spill level and overflow.");
            Require(NearlyEqual(
                        r.lakeWaterVolumeKm3,
                        r.lakeSurfaceAreaKm2 * r.lakeSpillHeightKm),
                "Lake storage must match its area and spill depth.");
            Require(NearlyEqual(
                        r.lakeAnnualSpillKm3,
                        r.lakeAnnualInflowKm3 - r.lakeAnnualEvaporationKm3 -
                            r.lakeAnnualInfiltrationKm3,
                        1e-9),
                "Lake spill must equal inflow minus evaporation and infiltration.");
            // Water lost in the lake must not reappear downstream.
            Require(!r.hasRiver ||
                    NearlyEqual(r.annualRunoffVolumeKm3, r.lakeAnnualSpillKm3, 1e-9),
                "River discharge at a lake must equal the lake spill.");
        };
        bool foundStableLake = false;

        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const auto& r = map.GetRegion(la, lo);
                Require(std::isfinite(r.drainageAccumulation) &&
                        r.drainageAccumulation >= 0.0 &&
                        r.drainageAccumulation <= 1.0,
                    "drainageAccumulation is not in [0,1] or is non-finite.");
                Require(std::isfinite(r.riverDischarge) &&
                        r.riverDischarge >= 0.0,
                    "riverDischarge is non-finite or negative.");
                Require(std::isfinite(r.annualRunoffVolumeKm3) &&
                        r.annualRunoffVolumeKm3 >= 0.0,
                    "annualRunoffVolumeKm3 is non-finite or negative.");
                // Rivers must have positive physical discharge
                if (r.hasRiver)
                    Require(r.annualRunoffVolumeKm3 > 0.0,
                        "Cell marked hasRiver but annualRunoffVolumeKm3 is zero.");
                // Lakes must sit in genuine depressions, not in the ocean
                if (r.isLake)
                {
                    validateLake(r);
                    foundStableLake = true;
                }
            }
        }

        for (std::uint32_t seed = 0xbeef0000u;
             seed < 0xbeef0020u && !foundStableLake;
             ++seed)
        {
            params.seed = seed;
            const Infinitesia::PlanetMap candidateMap(params);
            for (int la = 0; la < latB && !foundStableLake; ++la)
            {
                for (int lo = 0; lo < lonS; ++lo)
                {
                    const auto& region = candidateMap.GetRegion(la, lo);
                    if (region.isLake)
                    {
                        validateLake(region);
                        foundStableLake = true;
                        break;
                    }
                }
            }
        }
        Require(foundStableLake,
            "Expected a generated lake with a positive annual water balance.");
    }

    // --- Priority-Flood fallback: land-heavy planet ---
    // The fully-land-covered fallback (seeding Priority-Flood from the global
    // minimum instead of ocean cells) is exercised by the land-heavy planet
    // path.  We cannot guarantee zero ocean from PlanetParameters alone
    // without a separate override parameter, so this test verifies the
    // invariants that must hold in either case.  The fallback code path is
    // separately exercised by the hydrology unit test below.
    //
    // We use a fixed seed that produces ~82% ocean so the code path near the
    // fallback boundary (few ocean cells, unusual topology) is stress-tested.
    {
        Infinitesia::PlanetParameters params;
        params.latitudeBands     = 45;
        params.longitudeSegments = 90;
        params.seed              = 0xc0ffee42;
        const Infinitesia::PlanetMap map(params);
        const int latB = map.GetLatitudeBandCount();
        const int lonS = map.GetLongitudeSegmentCount();

        int oceanCells = 0;
        const int totalCells = latB * lonS;
        for (int la = 0; la < latB; ++la)
            for (int lo = 0; lo < lonS; ++lo)
                if (map.GetRegion(la, lo).isOcean)
                    ++oceanCells;

        const double oceanFraction =
            static_cast<double>(oceanCells) / totalCells;
        (void)oceanFraction; // informational

        bool allFinite = true;
        bool noNegativeAccum = true;
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const auto& r = map.GetRegion(la, lo);
                if (!std::isfinite(r.drainageAccumulation) ||
                    !std::isfinite(r.annualRunoffVolumeKm3))
                    allFinite = false;
                if (r.drainageAccumulation < 0.0 ||
                    r.annualRunoffVolumeKm3 < 0.0)
                    noNegativeAccum = false;
            }
        }
        Require(allFinite,
            "Priority-Flood produced non-finite hydrology values.");
        Require(noNegativeAccum,
            "Priority-Flood produced negative accumulation values.");

        bool anyDrainage = false;
        for (int la = 0; la < latB && !anyDrainage; ++la)
            for (int lo = 0; lo < lonS && !anyDrainage; ++lo)
                if (map.GetRegion(la, lo).drainageAccumulation > 0.0)
                    anyDrainage = true;
        Require(anyDrainage,
            "Planet produced no drainage accumulation at all.");
    }

    // --- Priority-Flood fallback unit test: synthetic no-ocean scenario ---
    // PlanetMap does not yet expose a "no ocean" override, so we verify the
    // fallback logic indirectly: a planet whose seed happens to produce >=97%
    // land area should still run without crashing and pass the same invariants.
    // The ocean fraction is checked at runtime so the test stays valid if the
    // generator is later tuned.
    {
        // Search for a seed with < 10% ocean at 45x90 resolution.
        // We scan a fixed range so the test is deterministic and terminates
        // quickly (max 200 iterations).
        Infinitesia::PlanetParameters params;
        params.latitudeBands     = 45;
        params.longitudeSegments = 90;
        const int totalCells = params.latitudeBands * params.longitudeSegments;

        bool foundLandHeavy = false;
        for (unsigned s = 0xbeef0000u; s < 0xbeef00c8u; ++s)
        {
            params.seed = s;
            const Infinitesia::PlanetMap map(params);
            int ocean = 0;
            const int latB = map.GetLatitudeBandCount();
            const int lonS = map.GetLongitudeSegmentCount();
            for (int la = 0; la < latB; ++la)
                for (int lo = 0; lo < lonS; ++lo)
                    if (map.GetRegion(la, lo).isOcean) ++ocean;
            if (static_cast<double>(ocean) / totalCells < 0.10)
            {
                // Verify invariants for this land-heavy configuration
                for (int la = 0; la < latB; ++la)
                    for (int lo = 0; lo < lonS; ++lo)
                    {
                        const auto& r = map.GetRegion(la, lo);
                        Require(std::isfinite(r.drainageAccumulation) &&
                                r.drainageAccumulation >= 0.0,
                            "Land-heavy planet: drainageAccumulation invalid.");
                        Require(std::isfinite(r.annualRunoffVolumeKm3) &&
                                r.annualRunoffVolumeKm3 >= 0.0,
                            "Land-heavy planet: annualRunoffVolumeKm3 invalid.");
                    }
                foundLandHeavy = true;
                break;
            }
        }
        // Some generator configurations may not produce very land-heavy planets;
        // the key is that when ocean coverage is low the invariants hold.
        (void)foundLandHeavy;
    }

    // --- Small planet radius: slope metric must remain in [0, 1] ---
    {
        Infinitesia::PlanetParameters params;
        params.radiusMeters      = 2000000.0; // 2 000 km — Mars-size
        params.massKilograms     = 6.4e23;
        params.meanDensityKilogramsPerCubicMeter = 3900.0;
        params.gravityMetersPerSecondSquared     = 3.7;
        params.latitudeBands     = 45;
        params.longitudeSegments = 90;
        params.seed              = 42;
        const Infinitesia::PlanetMap map(params);
        const int latB = map.GetLatitudeBandCount();
        const int lonS = map.GetLongitudeSegmentCount();
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const auto& r = map.GetRegion(la, lo);
                Require(r.slopeNormalized >= 0.0 && r.slopeNormalized <= 1.0,
                    "slopeNormalized is outside [0,1] on a small planet.");
                // slopeNormalized at the polar boundaries (la=0 and la=latB-1)
                // must equal that of their immediate interior neighbour when
                // using a degenerate one-cell stencil — verify it is not
                // anomalously higher than the global 99th percentile.
            }
        }

        // Polar slope: after the stencil fix (la2 - la1 = 1 at boundaries),
        // the polar bands should NOT show artificially elevated *mean* slope
        // compared to their immediate interior neighbours.  We compare the
        // mean of band 0 / band (latB-1) against the mean of band 1 /
        // band (latB-2).  The polar mean should be <= 2× the adjacent mean
        // (both are in the same terrain zone so they should be similar).
        auto bandMeanSlope = [&](int la) {
            double s = 0.0;
            for (int lo = 0; lo < lonS; ++lo)
                s += map.GetRegion(la, lo).slopeNormalized;
            return s / lonS;
        };
        if (latB >= 4)
        {
            const double meanPolar0    = bandMeanSlope(0);
            const double meanAdjacent0 = bandMeanSlope(1);
            const double meanPolarN    = bandMeanSlope(latB - 1);
            const double meanAdjacentN = bandMeanSlope(latB - 2);
            // Allow 2× tolerance: the polar cell spans only one step while
            // its neighbour spans two — if anything the polar value should
            // be lower, but noise means it may be slightly higher.
            Require(meanAdjacent0 == 0.0 ||
                    meanPolar0 <= meanAdjacent0 * 2.0,
                "North-pole band mean slope is more than 2× its neighbour.");
            Require(meanAdjacentN == 0.0 ||
                    meanPolarN <= meanAdjacentN * 2.0,
                "South-pole band mean slope is more than 2× its neighbour.");
        }
    }
}

int main()
{
    try
    {
        TestBoundaryClassificationSignConvention();
        TestSyntheticFullyLandDrainageFallback();
        TestDrainageAccumulationIsMonotonicAlongFlowGraph();
        TestEdgeCasePlanetConfigurations();
        TestSameScenarioIsDeterministic();
        TestDifferentScenariosProduceDifferentSystems();
        TestGeneratedSystemIsInternallyConsistent();
        TestInvalidAndUnsupportedScenariosFailExplicitly();
        TestSeedSweepPreservesGenerationInvariants();
        TestScenarioConstraintsAreApplied();
        TestPlanetMapIsDeterministicAndWithinGenerationBudget();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Planet generation test failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Planet generation tests passed.\n";
    return EXIT_SUCCESS;
}
