#pragma once

#include <cmath>
#include <cstdint>

namespace Infinitesia {
struct PlanetParameters {
  std::uint64_t planetId = 1;
  double radiusMeters = 6371000.0;
  double massKilograms = 5.9722e24;
  double meanDensityKilogramsPerCubicMeter = 5514.0;
  double gravityMetersPerSecondSquared = 9.81;
  double rotationHours = 24.0;
  double axialTiltDegrees = 23.44;
  double ageYears = 4.54e9;
  double bulkIronMassFraction = 0.32;
  double volatileMassFraction = 0.02;
  double atmospherePressureKPa = 101.325;
  std::uint32_t seed = 1;
  int latitudeBands = 360;  // 0.5 deg, ~55 km per cell
  int longitudeSegments = 720;

  bool IsValid() const {
    return std::isfinite(radiusMeters) && radiusMeters > 0.0 &&
           std::isfinite(massKilograms) && massKilograms > 0.0 &&
           std::isfinite(meanDensityKilogramsPerCubicMeter) &&
           meanDensityKilogramsPerCubicMeter > 0.0 &&
           std::isfinite(gravityMetersPerSecondSquared) &&
           gravityMetersPerSecondSquared > 0.0 &&
           std::isfinite(rotationHours) && rotationHours > 0.0 &&
           std::isfinite(axialTiltDegrees) && axialTiltDegrees >= 0.0 &&
           axialTiltDegrees <= 180.0 && std::isfinite(ageYears) &&
           ageYears >= 0.0 && std::isfinite(bulkIronMassFraction) &&
           bulkIronMassFraction >= 0.0 && bulkIronMassFraction <= 1.0 &&
           std::isfinite(volatileMassFraction) && volatileMassFraction >= 0.0 &&
           volatileMassFraction <= 1.0 &&
           std::isfinite(atmospherePressureKPa) &&
           atmospherePressureKPa > 0.0 && latitudeBands > 0 &&
           longitudeSegments > 0;
  }
};
}  // namespace Infinitesia
