#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/StarSystemGenerator.h"
#include "support/Require.h"

TEST_CASE("PlanetMapIsDeterministicAndWithinGenerationBudget") {
  const auto start = std::chrono::steady_clock::now();
  const Infinitesia::PlanetMap defaultMap;
  const auto elapsed = std::chrono::steady_clock::now() - start;
#ifdef NDEBUG
  constexpr auto budget = std::chrono::seconds(2);
#else
  // Unoptimized builds are an order of magnitude slower.
  constexpr auto budget = std::chrono::seconds(60);
#endif
  Require(elapsed < budget,
          "Default planet-map generation exceeded the startup budget.");

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
  for (int latitude = 0; latitude < defaultMap.GetLatitudeBandCount();
       ++latitude) {
    for (int longitude = 0; longitude < defaultMap.GetLongitudeSegmentCount();
         ++longitude) {
      const Infinitesia::PlanetRegion& region =
          defaultMap.GetRegion(latitude, longitude);
      const Infinitesia::PlanetRegion& eastRegion = defaultMap.GetRegion(
          latitude, (longitude + 1) % defaultMap.GetLongitudeSegmentCount());
      const double eastStep = std::abs(region.elevationNormalized -
                                       eastRegion.elevationNormalized) *
                              9.0;
      if (eastStep > maximumAdjacentElevationStepKm)
        maximumAdjacentElevationStepKm = eastStep;
      if (latitude + 1 < defaultMap.GetLatitudeBandCount()) {
        const Infinitesia::PlanetRegion& northRegion =
            defaultMap.GetRegion(latitude + 1, longitude);
        const double northStep = std::abs(region.elevationNormalized -
                                          northRegion.elevationNormalized) *
                                 9.0;
        if (northStep > maximumAdjacentElevationStepKm)
          maximumAdjacentElevationStepKm = northStep;
      }
      const double areaWeight =
          std::cos(region.latitudeDegrees * std::numbers::pi / 180.0);
      totalAreaWeight += areaWeight;
      meanTemperatureNumerator += areaWeight * region.temperatureCelsius;
      maximumSeasonalTemperatureRangeC = std::max(
          maximumSeasonalTemperatureRangeC, region.temperatureAnnualRangeC);
      if (!region.isOcean) {
        minimumLandOrographicFactor =
            std::min(minimumLandOrographicFactor, region.windMoistureFactor);
        maximumLandOrographicFactor =
            std::max(maximumLandOrographicFactor, region.windMoistureFactor);
      }
      if (region.tectonicStress > 0.6) highStressAreaWeight += areaWeight;
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
      if (!region.isOcean) landAreaWeight += areaWeight;
      if (region.tectonicPlateId >= 0) {
        if (plateCrustKinds.size() <=
            static_cast<std::size_t>(region.tectonicPlateId)) {
          plateCrustKinds.resize(
              static_cast<std::size_t>(region.tectonicPlateId) + 1);
        }
        auto& [hasOceanicCrust, hasContinentalCrust] =
            plateCrustKinds[static_cast<std::size_t>(region.tectonicPlateId)];
        hasOceanicCrust |=
            region.surfaceRock == Infinitesia::RockType::BasalticOceanic;
        hasContinentalCrust |=
            region.surfaceRock != Infinitesia::RockType::BasalticOceanic;
      }
      Require(std::isfinite(region.elevationNormalized) &&
                  region.elevationNormalized >= -1.0 &&
                  region.elevationNormalized <= 1.0,
              "Generated elevation is not finite or normalized.");
      Require(std::isfinite(region.crustAge) && region.crustAge >= 0.0 &&
                  region.crustAge <= 1.0,
              "Generated crust age is outside its normalized range.");
      Require(
          std::isfinite(region.crustThickness) && region.crustThickness > 0.0,
          "Generated crust thickness is not physically positive.");
      Require(
          std::isfinite(region.isostaticCrustThicknessKm) &&
              region.isostaticCrustThicknessKm > 0.0,
          "Generated isostatic crust thickness is not physically positive.");
      Require(std::isfinite(region.tectonicStress) &&
                  region.tectonicStress >= 0.0 && region.tectonicStress <= 1.0,
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
           month < region.monthlyTemperatureCelsius.size(); ++month) {
        Require(std::isfinite(region.monthlyTemperatureCelsius[month]) &&
                    std::isfinite(region.monthlyPrecipitationMm[month]) &&
                    region.monthlyPrecipitationMm[month] >= 0.0,
                "Monthly climate contains invalid values.");
        monthlyTemperatureMean +=
            region.monthlyTemperatureCelsius[month] / 12.0;
        monthlyPrecipitationTotal += region.monthlyPrecipitationMm[month];
        minimumMonthlyTemperature = std::min(
            minimumMonthlyTemperature, region.monthlyTemperatureCelsius[month]);
        maximumMonthlyTemperature = std::max(
            maximumMonthlyTemperature, region.monthlyTemperatureCelsius[month]);
        dryMonthCount += region.monthlyPrecipitationMm[month] < 30.0 ? 1 : 0;
      }
      Require(
          std::abs(monthlyTemperatureMean - region.temperatureCelsius) < 1e-8 &&
              std::abs(monthlyPrecipitationTotal - region.precipitationMm) <
                  std::max(1e-8, region.precipitationMm * 1e-8) &&
              std::abs(maximumMonthlyTemperature - minimumMonthlyTemperature -
                       region.temperatureAnnualRangeC) < 1e-8 &&
              dryMonthCount == static_cast<std::size_t>(region.drySeasonMonths),
          "Annual climate does not agree with its monthly values.");
      Require(std::isfinite(region.windMoistureFactor) &&
                  region.windMoistureFactor >= 0.25 &&
                  region.windMoistureFactor <= 1.8,
              "Wind-driven moisture transfer is outside its calibrated range.");
      Require(region.soilFertility >= 0.0 && region.soilFertility <= 1.0 &&
                  region.soilDepthNorm >= 0.0 && region.soilDepthNorm <= 1.0,
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
  Require(
      meanTemperatureCelsius > 8.0 && meanTemperatureCelsius < 22.0,
      "Earth-like atmosphere produced an implausible global mean temperature.");
  Require(highStressAreaWeight / totalAreaWeight < 0.05,
          "Tectonic stress still covers an implausibly broad part of the map.");
  Require(transitionalCrustCells > 0,
          "Continental margins have no gradual crustal transition.");
  Require(broadReliefCells > 0,
          "Tectonic relief is confined to sharp stress-line cores.");
  Require(
      maximumTectonicUpliftKm > 2.0 && minimumTectonicReliefKm < -1.0,
      "Convergent and divergent boundaries lack broad mountain/trench relief.");
  Require(maximumAdjacentElevationStepKm < 3.0,
          "Adjacent terrain cells still form abrupt tectonic cliffs.");
  Require(
      maximumSeasonalTemperatureRangeC > 8.0,
      "Seasonal insolation is not producing a meaningful temperature range.");
  Require(
      minimumLandOrographicFactor < 0.9 && maximumLandOrographicFactor > 1.0,
      "Windward uplift and lee-side drying are not affecting precipitation.");
  Require(std::any_of(plateCrustKinds.begin(), plateCrustKinds.end(),
                      [](const auto& crustKinds) {
                        return crustKinds.first && crustKinds.second;
                      }),
          "Continental crust is still rigidly assigned by tectonic plate.");

  const Infinitesia::StarSystem seededSystem =
      Infinitesia::StarSystemGenerator::Generate(
          {.seed = 1, .generatorVersion = 2});
  const Infinitesia::PlanetMap seededMap(
      seededSystem.planet,
      Infinitesia::ClimateInputs{
          seededSystem.planetOrbit.meanIncidentFluxInEarthUnits,
          seededSystem.planet.axialTiltDegrees,
          seededSystem.planet.atmospherePressureKPa,
          seededSystem.planet.volatileMassFraction,
          seededSystem.planet.gravityMetersPerSecondSquared,
          seededSystem.planet.rotationHours, seededSystem.planet.ageYears});
  const double seedOneMidLatitudeC =
      seededMap.GetRegion(136, 254).temperatureCelsius;
  Require(seedOneMidLatitudeC > -13.0,
          "The original seed-1 mid-latitude climate is still over-cooled.");

  const Infinitesia::StarSystem currentSeedOne =
      Infinitesia::StarSystemGenerator::Generate({.seed = 1});
  const Infinitesia::PlanetMap currentSeedOneMap(
      currentSeedOne.planet,
      Infinitesia::ClimateInputs{
          currentSeedOne.planetOrbit.meanIncidentFluxInEarthUnits,
          currentSeedOne.planet.axialTiltDegrees,
          currentSeedOne.planet.atmospherePressureKPa,
          currentSeedOne.planet.volatileMassFraction,
          currentSeedOne.planet.gravityMetersPerSecondSquared,
          currentSeedOne.planet.rotationHours, currentSeedOne.planet.ageYears});
  std::array<std::size_t, 25> biomeCounts{};
  double meanPrecipitation = 0.0;
  std::size_t totalCells = 0;
  for (int latitude = 0; latitude < currentSeedOneMap.GetLatitudeBandCount();
       ++latitude) {
    for (int longitude = 0;
         longitude < currentSeedOneMap.GetLongitudeSegmentCount();
         ++longitude) {
      const auto& region = currentSeedOneMap.GetRegion(latitude, longitude);
      ++biomeCounts[static_cast<std::size_t>(region.biome)];
      meanPrecipitation += region.precipitationMm;
      ++totalCells;
    }
  }
  const auto biomeArea = [&biomeCounts](Infinitesia::Biome biome) {
    return biomeCounts[static_cast<std::size_t>(biome)];
  };
  std::size_t distinctLandBiomes = 0;
  for (std::size_t index = 4; index < biomeCounts.size(); ++index)
    distinctLandBiomes += biomeCounts[index] > 0 ? 1 : 0;
  Require(distinctLandBiomes >= 8,
          "The Earth-like seed produces too few distinct land biomes.");
  Require(
      biomeArea(Infinitesia::Biome::TropicalRainforest) > 0 &&
          biomeArea(Infinitesia::Biome::TemperateDeciduous) > 0 &&
          (biomeArea(Infinitesia::Biome::Savanna) > 0 ||
           biomeArea(Infinitesia::Biome::HotDesert) > 0),
      "The seed-1 world is missing tropical, temperate, or warm-dry biomes.");
  Require(biomeArea(Infinitesia::Biome::TropicalMonsoonForest) > 0 ||
              biomeArea(Infinitesia::Biome::TemperateRainforest) > 0,
          "Seasonal climate did not produce any newly differentiated forest "
          "biome.");
  Require(
      meanPrecipitation / totalCells > 250.0,
      "The Earth-like climate still has globally suppressed precipitation.");

  Infinitesia::PlanetParameters parameters;
  parameters.latitudeBands = 45;
  parameters.longitudeSegments = 90;
  parameters.seed = 2026;
  const Infinitesia::PlanetMap first(parameters);
  const Infinitesia::PlanetMap second(parameters);
  for (int latitude = 0; latitude < parameters.latitudeBands; ++latitude) {
    for (int longitude = 0; longitude < parameters.longitudeSegments;
         ++longitude) {
      const Infinitesia::PlanetRegion& firstRegion =
          first.GetRegion(latitude, longitude);
      const Infinitesia::PlanetRegion& secondRegion =
          second.GetRegion(latitude, longitude);
      Require(
          firstRegion.tectonicPlateId == secondRegion.tectonicPlateId &&
              firstRegion.crustAge == secondRegion.crustAge &&
              firstRegion.elevationNormalized ==
                  secondRegion.elevationNormalized &&
              firstRegion.biome == secondRegion.biome,
          "Planet-map generation is not deterministic for an identical seed.");
    }
  }
}
