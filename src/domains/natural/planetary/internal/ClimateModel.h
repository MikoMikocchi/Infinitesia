#pragma once

// Physical helpers of the climate and hydrology stages.
namespace Infinitesia::GenerationDetail
{
double InsolationFraction(double latitudeDegrees, double axialTiltDegrees);
double SeasonalAmplitude(double latitudeDegrees, double axialTiltDegrees, double rotationHours);
double EquilibriumTempK(double incidentFlux);
double GreenhouseK(double pressureKPa);
double LapseK(double elevationNormalized);
// Annual evapotranspiration demand [mm/yr]; shared by climate and hydrology.
double AnnualEvapotranspirationMm(double temperatureCelsius);
double CirculationWetness(double absoluteLatitudeDegrees, double rotationHours);
double DrySeasonMonths(double precipitationMm, double circulationWetness);
} // namespace Infinitesia::GenerationDetail
