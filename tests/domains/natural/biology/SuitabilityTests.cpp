#include <doctest/doctest.h>

#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/biology/niche/Suitability.h"
#include "support/MaterialFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace {
// A niche with a known shape on every axis.
Niche TestNiche() {
  Niche niche;
  niche.temperatureCelsius = {0.0, 10.0, 20.0, 30.0};
  niche.precipitationMm = {200.0, 600.0, 1200.0, 2000.0};
  niche.elevationMeters = {-10.0, 0.0, 500.0, 1500.0};
  niche.maxDrySeasonMonths = 3.0;
  niche.minSoilFertility = 0.4;
  niche.minSoilDepth = 0.2;
  niche.maxSlope = 1.0;
  return niche;
}

// Conditions on the optimum of every axis of TestNiche().
Conditions Ideal() {
  Conditions conditions;
  conditions.temperatureCelsius = 15.0;
  conditions.precipitationMm = 900.0;
  conditions.elevationMeters = 100.0;
  conditions.drySeasonMonths = 2.0;
  conditions.soilFertility = 0.8;
  conditions.soilDepth = 0.5;
  conditions.slope = 0.2;
  return conditions;
}
}  // namespace

TEST_CASE("ToleranceIsATrapezoid") {
  const Trapezoid trapezoid{0.0, 10.0, 20.0, 30.0};
  CHECK(Tolerance(trapezoid, -5.0) == 0.0);
  CHECK(Tolerance(trapezoid, 0.0) == 0.0);
  CHECK(NearlyEqual(Tolerance(trapezoid, 5.0), 0.5));
  CHECK(Tolerance(trapezoid, 10.0) == 1.0);
  CHECK(Tolerance(trapezoid, 15.0) == 1.0);
  CHECK(Tolerance(trapezoid, 20.0) == 1.0);
  CHECK(NearlyEqual(Tolerance(trapezoid, 25.0), 0.5));
  CHECK(Tolerance(trapezoid, 30.0) == 0.0);
  CHECK(Tolerance(trapezoid, 45.0) == 0.0);
}

TEST_CASE("ConditionsOnTheOptimumSuitTheSpeciesFully") {
  CHECK(Suitability(TestNiche(), Ideal()) == 1.0);
}

TEST_CASE("OneAxisAtZeroRulesTheSpeciesOut") {
  const Niche niche = TestNiche();
  Conditions cold = Ideal();
  cold.temperatureCelsius = -3.0;
  CHECK(Suitability(niche, cold) == 0.0);

  Conditions dry = Ideal();
  dry.precipitationMm = 100.0;
  CHECK(Suitability(niche, dry) == 0.0);

  Conditions high = Ideal();
  high.elevationMeters = 2000.0;
  CHECK(Suitability(niche, high) == 0.0);

  Conditions droughty = Ideal();
  droughty.drySeasonMonths = 4.5;
  CHECK(Suitability(niche, droughty) == 0.0);

  Conditions barren = Ideal();
  barren.soilFertility = 0.0;
  CHECK(Suitability(niche, barren) == 0.0);

  Conditions shallow = Ideal();
  shallow.soilDepth = 0.0;
  CHECK(Suitability(niche, shallow) == 0.0);

  Conditions cliff = Ideal();
  cliff.slope = 1.0;
  CHECK(Suitability(niche, cliff) == 0.0);
}

TEST_CASE("AxesCombineByProduct") {
  const Niche niche = TestNiche();
  Conditions conditions = Ideal();
  conditions.temperatureCelsius = 5.0;  // 0.5
  conditions.precipitationMm = 1600.0;  // 0.5
  CHECK(NearlyEqual(Suitability(niche, conditions), 0.25));

  conditions.soilFertility = 0.2;  // 0.5 more
  CHECK(NearlyEqual(Suitability(niche, conditions), 0.125));
}

TEST_CASE("SoftLimitsFallLinearly") {
  const Niche niche = TestNiche();
  Conditions conditions = Ideal();

  conditions.drySeasonMonths = 3.5;
  CHECK(NearlyEqual(Suitability(niche, conditions), 0.5));

  conditions = Ideal();
  conditions.slope = 0.8;  // between 0.6 and 1.0
  CHECK(NearlyEqual(Suitability(niche, conditions), 0.5));

  conditions = Ideal();
  conditions.soilDepth = 0.1;  // half of the minimum
  CHECK(NearlyEqual(Suitability(niche, conditions), 0.5));
}

TEST_CASE("ALimitOfZeroIsNotAConstraint") {
  Niche niche = TestNiche();
  niche.minSoilFertility = 0.0;
  niche.minSoilDepth = 0.0;
  Conditions conditions = Ideal();
  conditions.soilFertility = 0.0;
  conditions.soilDepth = 0.0;
  CHECK(Suitability(niche, conditions) == 1.0);
}

TEST_CASE("SuitabilityIsDeterministicAndBounded") {
  const Niche niche = TestNiche();
  for (double temperature = -10.0; temperature <= 40.0; temperature += 2.5) {
    for (double rain = 0.0; rain <= 3000.0; rain += 250.0) {
      Conditions conditions = Ideal();
      conditions.temperatureCelsius = temperature;
      conditions.precipitationMm = rain;
      const double value = Suitability(niche, conditions);
      Require(value >= 0.0 && value <= 1.0, "suitability must be within 0..1");
      Require(value == Suitability(niche, conditions),
              "suitability must be repeatable");
    }
  }
}

TEST_CASE("ShippedSpeciesPreferTheirOwnClimate") {
  const SpeciesCatalog catalog = SpeciesCatalog::Load(
      kAssets / "biology" / "species.json", *SharedMaterials());
  const Niche& oak = catalog.Find("species.oak")->niche;
  const Niche& spruce = catalog.Find("species.spruce")->niche;
  const Niche& palm = catalog.Find("species.palm")->niche;

  Conditions temperate = Ideal();
  temperate.temperatureCelsius = 12.0;
  temperate.precipitationMm = 900.0;
  temperate.elevationMeters = 200.0;
  temperate.drySeasonMonths = 1.0;
  temperate.soilFertility = 0.6;
  temperate.soilDepth = 0.5;
  temperate.slope = 0.1;
  CHECK(Suitability(oak, temperate) > Suitability(spruce, temperate));
  CHECK(Suitability(palm, temperate) == 0.0);

  Conditions boreal = temperate;
  boreal.temperatureCelsius = 1.0;
  CHECK(Suitability(spruce, boreal) > Suitability(oak, boreal));

  Conditions tropical = temperate;
  tropical.temperatureCelsius = 26.0;
  tropical.precipitationMm = 1800.0;
  tropical.elevationMeters = 50.0;
  CHECK(Suitability(palm, tropical) > 0.9);
  CHECK(Suitability(spruce, tropical) == 0.0);
}
