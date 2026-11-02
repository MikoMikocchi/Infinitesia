#include "planet/StarSystemGenerator.h"
#include "planet/PlanetMap.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
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
        Require(system.scenario.generatorVersion == 5,
                "The revised planet-generation model did not use generator version 5.");
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

        bool detailedModeRejected = false;
        try
        {
            Infinitesia::StarSystemGenerator::Generate({
                .seed = 1,
                .generatorVersion = 1,
                .mode = Infinitesia::GenerationMode::Detailed
            });
        }
        catch (const std::invalid_argument&)
        {
            detailedModeRejected = true;
        }
        Require(detailedModeRejected, "Unsupported detailed mode was not rejected.");
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
        std::array<std::size_t, 23> biomeCounts{};
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

int main()
{
    try
    {
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
