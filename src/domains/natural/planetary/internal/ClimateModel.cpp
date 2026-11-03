#include "domains/natural/planetary/internal/ClimateModel.h"
#include "domains/natural/planetary/PlanetRegion.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia::GenerationDetail
{

// --------------------------------------------------------------------------
// Climate helpers
// --------------------------------------------------------------------------

// Annual-mean insolation fraction vs latitude + axial tilt
double InsolationFraction(double latDeg, double tiltDeg)
{
    const double latitude = latDeg * std::numbers::pi / 180.0;
    const double tilt = tiltDeg * std::numbers::pi / 180.0;
    const double sinLatitude = std::sin(latitude);
    const double cosLatitude = std::cos(latitude);
    double seasonalMean = 0.0;
    constexpr int kSeasonSamples = 64;

    for (int sample = 0; sample < kSeasonSamples; ++sample)
    {
        const double orbitalLongitude =
            2.0 * std::numbers::pi *
            (static_cast<double>(sample) + 0.5) / kSeasonSamples;
        const double declination =
            std::asin(std::sin(tilt) * std::sin(orbitalLongitude));
        const double hourAngleArgument =
            -std::tan(latitude) * std::tan(declination);
        const double sunsetHourAngle = hourAngleArgument <= -1.0
            ? std::numbers::pi
            : hourAngleArgument >= 1.0
                ? 0.0
                : std::acos(hourAngleArgument);
        const double dailyMean =
            sunsetHourAngle * sinLatitude * std::sin(declination) +
            cosLatitude * std::cos(declination) *
                std::sin(sunsetHourAngle);
        seasonalMean += dailyMean;
    }

    // Normalize by the spherical global mean (one quarter of the solar constant).
    return std::max(
        0.0,
        (4.0 / std::numbers::pi) * seasonalMean / kSeasonSamples);
}

// Seasonal amplitude: difference summer–winter temperature [°C]
double SeasonalAmplitude(double latDeg, double tiltDeg, double rotHours)
{
    const double lat  = std::abs(latDeg) * std::numbers::pi / 180.0;
    const double tilt = tiltDeg * std::numbers::pi / 180.0;
    const double baseAmplitude = std::sin(lat) * std::sin(tilt) * 50.0;
    const double rotFactor = std::clamp(std::log(rotHours / 24.0 + 1.0) / 1.0, 0.5, 1.5);
    return std::clamp(baseAmplitude * rotFactor, 0.0, 60.0);
}

// Physically-grounded equilibrium temperature.
//
// Stefan-Boltzmann for a planet with a given Bond albedo:
//   T_eff = 278.5 K × (1 - A)^0.25 × flux^0.25
//
// 278.5 K = (S₀ / 4σ)^0.25 where S₀ = 1361 W/m² and σ = 5.67e-8 W/m²/K⁴.
// For Earth (A = 0.30, flux = 1.0) this gives 255 K, matching observations.
//
// The Bond albedo is kept at the Earth-like default of 0.30 here.
// A future improvement would vary it with ice coverage and cloud fraction
// derived from the climate system.  For now, the fixed value is documented
// as a known approximation (see ClimateInputs).
double EquilibriumTempK(double flux)
{
    // Bond albedo A = 0.30 (Earth-like; fixed approximation — see comment above).
    constexpr double kBondAlbedo = 0.30;
    // 278.5 K = (S₀ / 4σ)^0.25
    constexpr double kStefanBase = 278.5;
    return kStefanBase *
        std::pow(1.0 - kBondAlbedo, 0.25) *
        std::pow(std::max(0.01, flux), 0.25);
}

// Greenhouse warming from atmospheric pressure.
//
// APPROXIMATION: this function models the greenhouse effect as a function of
// total surface pressure alone, calibrated to reproduce Earth's ~33 K warming
// at 101.325 kPa.  It does NOT account for:
//   • Atmospheric composition (CO₂ fraction, CH₄, H₂O vapour pressure).
//   • Spectroscopic line strengths of individual greenhouse gases.
//   • volatileMassFraction from ClimateInputs (stored but currently unused).
//
// The √(pressure) scaling is a rough proxy; real greenhouse sensitivity is
// logarithmic in CO₂ concentration.  This is acceptable for an Earth-like
// default scenario but will be inaccurate for high-CO₂ or pure-N₂ atmospheres.
// A future improvement: split the contribution into a CO₂ term (logarithmic)
// and a water-vapour feedback term (temperature-dependent).
double GreenhouseK(double pressKPa)
{
    if (pressKPa <= 0.0) return 0.0;
    return 33.0 * std::sqrt(pressKPa / 101.325);
}

// Altitude lapse cooling [K]
double LapseK(double elevNorm)
{
    const double elevM = std::max(0.0, elevNorm) * kMaxElevationKm * 1000.0;
    return elevM * 0.0065;
}

double AnnualEvapotranspirationMm(double temperatureCelsius)
{
    return std::clamp(120.0 + temperatureCelsius * 12.0, 0.0, 600.0);
}

// Zonal atmospheric circulation wetness [0..1]
// Models Hadley / Ferrel / Polar cells with rotation-period scaling.
double CirculationWetness(double absLatDeg, double rotHours)
{
    const double cellScale = std::clamp(24.0 / rotHours, 0.4, 2.0);
    const double lat = absLatDeg * std::numbers::pi / 180.0;

    // ITCZ: ascending moist air near equator
    const double itczWidth = 0.12 * cellScale;
    const double itcz = std::exp(-std::pow(lat, 2.0) / itczWidth);

    // Subtropical dry belt (~25°): descending dry air
    const double subC = 25.0 / cellScale * std::numbers::pi / 180.0;
    const double sub  = std::exp(-std::pow(lat - subC, 2.0) / 0.04);

    // Mid-latitude frontal zone (~55°): ascending moist air
    const double midC = 55.0 / cellScale * std::numbers::pi / 180.0;
    const double mid  = 0.60 * std::exp(-std::pow(lat - midC, 2.0) / 0.05);

    // Polar dry (>65°): descending dry air
    const double polarDry = std::max(0.0, (absLatDeg - 65.0) / 25.0);

    const double raw = itcz - sub * 0.75 + mid - polarDry * 0.5;
    return std::clamp(raw * 0.5 + 0.5, 0.0, 1.0);
}

// Dry-season months derived from the circulation wetness signal
double DrySeasonMonths(double /*precipMm*/, double circulationWetness)
{
    const double dryFraction = std::clamp(1.0 - circulationWetness * 1.4, 0.0, 1.0);
    return dryFraction * 7.0;
}

}
