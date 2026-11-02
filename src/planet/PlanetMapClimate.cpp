#include "planet/PlanetMap.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia
{
    using namespace GenerationDetail;

    void PlanetMap::GenerateClimate()
    {
        const int latB = LatBands();
        const int lonS = LonSegs();
        const std::uint32_t seed = m_planet.GetSeed();

        const double flux = m_climate.meanIncidentFluxInEarthUnits;
        const double tilt = m_climate.axialTiltDegrees;
        const double rot = m_climate.rotationPeriodHours;
        const double pressKPa = m_climate.atmospherePressureKPa;

        const double globalEquilK = EquilibriumTempK(flux);
        const double greenhouse = GreenhouseK(pressKPa);
        const double pressureScale =
            std::sqrt(std::clamp(pressKPa / 101.325, 0.1, 4.0));
        double areaWeightSum = 0.0;
        double meanInsolation = 0.0;
        std::vector<double> bandInsolation(static_cast<std::size_t>(latB));
        for (int la = 0; la < latB; ++la)
        {
            const double latitude =
                (static_cast<double>(la) + 0.5) / latB * 180.0 - 90.0;
            const double areaWeight =
                std::max(0.0, std::cos(latitude * std::numbers::pi / 180.0));
            bandInsolation[static_cast<std::size_t>(la)] =
                InsolationFraction(latitude, tilt);
            areaWeightSum += areaWeight;
            meanInsolation +=
                areaWeight * bandInsolation[static_cast<std::size_t>(la)];
        }
        meanInsolation /= areaWeightSum;

        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                auto& c = Cell(la, lo);
                const double insolation = bandInsolation[static_cast<std::size_t>(la)];
                constexpr double kLatitudinalHeatRedistribution = 55.0;
                const double latitudeTempK = globalEquilK + greenhouse +
                    kLatitudinalHeatRedistribution * (insolation - meanInsolation);
                const double lapseK = LapseK(c.elevationNormalized);
                const double noisePerturbation =
                    (SphericalFbm(c.latitudeDegrees, c.longitudeDegrees,
                                  seed ^ 0xaa55cc77, 5, 4) - 0.5) * 8.0;

                c.temperatureCelsius =
                    latitudeTempK - 273.15 - lapseK + noisePerturbation;
                c.temperatureAnnualRangeC =
                    SeasonalAmplitude(c.latitudeDegrees, tilt, rot) *
                    (c.isOcean ? 0.4 : 1.0);
                c.continentality = 0.0;
            }
        }

        const int windowLat = std::max(1, latB / 18);
        const int windowLon = std::max(1, lonS / 36);
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                double landArea = 0.0;
                double totalArea = 0.0;
                for (int dla = -windowLat; dla <= windowLat; ++dla)
                {
                    for (int dlo = -windowLon; dlo <= windowLon; ++dlo)
                    {
                        const int nla = std::clamp(la + dla, 0, latB - 1);
                        const int nlo = ((lo + dlo) + lonS) % lonS;
                        const auto& neighbor = Cell(nla, nlo);
                        const double areaWeight =
                            std::max(0.0, std::cos(
                                neighbor.latitudeDegrees * std::numbers::pi / 180.0));
                        if (!neighbor.isOcean)
                            landArea += areaWeight;
                        totalArea += areaWeight;
                    }
                }
                Cell(la, lo).continentality =
                    totalArea > 0.0 ? landArea / totalArea : 0.0;
            }
        }

        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                auto& c = Cell(la, lo);
                const double absoluteLatitude = std::abs(c.latitudeDegrees);
                const double circulation = CirculationWetness(absoluteLatitude, rot);
                const double continentalDryness = 1.0 - c.continentality * 0.6;
                const double moistureNoise =
                    SphericalFbm(c.latitudeDegrees, c.longitudeDegrees,
                                 seed ^ 0x1234abcd, 5, 5);
                const double orographic = c.isOcean ? 0.0
                    : std::max(0.0, c.slopeNormalized * 0.5 - c.tectonicStress * 0.1);

                c.humidityNormalized = std::clamp(
                    circulation * std::sqrt(circulation) * continentalDryness *
                    (0.5 + moistureNoise * 0.5) * (1.0 + orographic),
                    0.0, 1.0);

                const double saturationFactor =
                    std::clamp(
                        std::exp((c.temperatureCelsius - 15.0) * 0.055),
                        0.15,
                        2.5);
                c.precipitationMm = std::max(
                    0.0, c.humidityNormalized * 3200.0 *
                             pressureScale * saturationFactor);

                if (!c.isOcean && c.slopeNormalized > 0.3 && circulation < 0.45)
                    c.precipitationMm *=
                        std::clamp(1.0 - c.slopeNormalized * 0.7, 0.2, 1.0);

                const double potentialEvapotranspiration =
                    std::max(0.0, c.temperatureCelsius * 15.0);
                c.soilMoisture = std::clamp(
                    c.precipitationMm /
                        (c.precipitationMm + potentialEvapotranspiration + 1.0),
                    0.0, 1.0);
                c.drySeasonMonths = DrySeasonMonths(c.precipitationMm, circulation);

                const double snowTemp =
                    std::clamp(-c.temperatureCelsius / 10.0, 0.0, 1.0);
                c.snowDays = snowTemp *
                    std::clamp(c.precipitationMm / 800.0, 0.0, 1.0) * 200.0;
            }
        }
    }
}
