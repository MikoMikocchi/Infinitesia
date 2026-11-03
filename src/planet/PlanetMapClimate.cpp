#include "planet/PlanetMap.h"
#include "planet/PlanetLoadSteps.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <vector>

namespace Infinitesia
{
    namespace
    {
        constexpr int kMonthsPerYear = 12;

        double MonthlyInsolation(double latitudeDegrees, double tiltDegrees, int month)
        {
            const double latitude = latitudeDegrees * std::numbers::pi / 180.0;
            const double tilt = tiltDegrees * std::numbers::pi / 180.0;
            const double seasonalLongitude =
                2.0 * std::numbers::pi *
                (static_cast<double>(month) + 0.5) / kMonthsPerYear -
                std::numbers::pi / 2.0;
            const double declination =
                std::asin(std::sin(tilt) * std::sin(seasonalLongitude));
            const double sunsetArgument =
                -std::tan(latitude) * std::tan(declination);
            const double sunsetAngle = sunsetArgument <= -1.0
                ? std::numbers::pi
                : sunsetArgument >= 1.0
                    ? 0.0
                    : std::acos(sunsetArgument);
            return std::max(
                0.0,
                sunsetAngle * std::sin(latitude) * std::sin(declination) +
                    std::cos(latitude) * std::cos(declination) *
                        std::sin(sunsetAngle));
        }

        double SeasonalWetness(
            double latitudeDegrees,
            double declinationDegrees,
            double rotationHours)
        {
            const double relativeLatitude =
                std::abs(latitudeDegrees - declinationDegrees);
            const double itcz =
                std::exp(-std::pow((latitudeDegrees - declinationDegrees) / 17.0, 2.0));
            const double subtropicalDryness =
                std::exp(-std::pow((relativeLatitude - 27.0) / 13.0, 2.0));
            const double midLatitudeStorms =
                std::exp(-std::pow((relativeLatitude - 52.0) / 20.0, 2.0));
            const double polarDryness =
                std::clamp((relativeLatitude - 68.0) / 25.0, 0.0, 1.0);
            const double circulation =
                GenerationDetail::CirculationWetness(
                    std::abs(latitudeDegrees), rotationHours);
            return std::clamp(
                circulation * (0.62 + 0.55 * itcz + 0.28 * midLatitudeStorms -
                               0.25 * subtropicalDryness - 0.2 * polarDryness),
                0.08,
                1.5);
        }
    }

    using namespace GenerationDetail;

    void PlanetMap::GenerateClimate()
    {
        const int latB = LatBands();
        const int lonS = LonSegs();
        const std::uint32_t seed = m_planet.GetSeed();
        const double tilt = m_climate.axialTiltDegrees;
        const double rot = m_climate.rotationPeriodHours;
        const double pressKPa = m_climate.atmospherePressureKPa;
        const double pressureScale =
            std::sqrt(std::clamp(pressKPa / 101.325, 0.1, 4.0));
        const double globalEquilK =
            EquilibriumTempK(m_climate.meanIncidentFluxInEarthUnits);
        const double greenhouse = GreenhouseK(pressKPa);
        std::vector<std::array<double, kMonthsPerYear>> monthlyInsolation(
            static_cast<std::size_t>(latB));
        std::array<double, kMonthsPerYear> meanMonthlyInsolation{};
        std::vector<double> areaWeights(static_cast<std::size_t>(latB));
        double areaWeightSum = 0.0;

        for (int latitude = 0; latitude < latB; ++latitude)
        {
            const double latitudeDegrees =
                (static_cast<double>(latitude) + 0.5) / latB * 180.0 - 90.0;
            const double areaWeight = std::max(
                0.0,
                std::cos(latitudeDegrees * std::numbers::pi / 180.0));
            areaWeights[static_cast<std::size_t>(latitude)] = areaWeight;
            areaWeightSum += areaWeight;
            for (int month = 0; month < kMonthsPerYear; ++month)
            {
                const double insolation =
                    MonthlyInsolation(latitudeDegrees, tilt, month);
                monthlyInsolation[static_cast<std::size_t>(latitude)]
                                 [static_cast<std::size_t>(month)] = insolation;
                meanMonthlyInsolation[static_cast<std::size_t>(month)] +=
                    areaWeight * insolation;
            }
        }
        for (double& mean : meanMonthlyInsolation)
            mean /= areaWeightSum;

        const int windowLat = std::max(1, latB / 18);
        const int windowLon = std::max(1, lonS / 36);
        for (int latitude = 0; latitude < latB; ++latitude)
        {
            for (int longitude = 0; longitude < lonS; ++longitude)
            {
                double landArea = 0.0;
                double totalArea = 0.0;
                for (int latitudeOffset = -windowLat;
                     latitudeOffset <= windowLat;
                     ++latitudeOffset)
                {
                    for (int longitudeOffset = -windowLon;
                         longitudeOffset <= windowLon;
                         ++longitudeOffset)
                    {
                        const int neighborLatitude =
                            std::clamp(latitude + latitudeOffset, 0, latB - 1);
                        const int neighborLongitude =
                            (longitude + longitudeOffset + lonS) % lonS;
                        const auto& neighbor =
                            Cell(neighborLatitude, neighborLongitude);
                        const double areaWeight = std::max(
                            0.0,
                            std::cos(neighbor.latitudeDegrees *
                                     std::numbers::pi / 180.0));
                        if (!neighbor.isOcean)
                            landArea += areaWeight;
                        totalArea += areaWeight;
                    }
                }
                Cell(latitude, longitude).continentality =
                    totalArea > 0.0 ? landArea / totalArea : 0.0;
            }
            ReportRow(latitude, latB);
        }

        BeginLoadStep(m_progress, LoadStep::kClimateTemperature, "Simulating climate: temperature");
        std::vector<double> annualInsolation(static_cast<std::size_t>(latB));
        double meanAnnualInsolation = 0.0;
        for (int latitude = 0; latitude < latB; ++latitude)
        {
            double annual = 0.0;
            for (int month = 0; month < kMonthsPerYear; ++month)
            {
                const double monthlyMean =
                    meanMonthlyInsolation[static_cast<std::size_t>(month)];
                const double normalized = monthlyMean > 0.0
                    ? monthlyInsolation[static_cast<std::size_t>(latitude)]
                                       [static_cast<std::size_t>(month)] /
                          monthlyMean
                    : 0.0;
                annual += normalized / kMonthsPerYear;
            }
            annualInsolation[static_cast<std::size_t>(latitude)] = annual;
            meanAnnualInsolation +=
                areaWeights[static_cast<std::size_t>(latitude)] * annual;
        }
        meanAnnualInsolation /= areaWeightSum;

        for (int latitude = 0; latitude < latB; ++latitude)
        {
            for (int longitude = 0; longitude < lonS; ++longitude)
            {
                auto& cell = Cell(latitude, longitude);
                const double baseTemperatureC =
                    globalEquilK + greenhouse -
                    273.15 +
                    55.0 * (annualInsolation[static_cast<std::size_t>(latitude)] -
                            meanAnnualInsolation) -
                    LapseK(cell.elevationNormalized) +
                    (SphericalFbm(
                         cell.latitudeDegrees,
                         cell.longitudeDegrees,
                         seed ^ 0xaa55cc77,
                         5,
                         4) -
                     0.5) *
                        8.0;
                const double thermalDamping = cell.isOcean
                    ? 0.28
                    : 0.48 + 0.45 * cell.continentality;
                double meanTemperature = 0.0;
                double minimumTemperature = std::numeric_limits<double>::infinity();
                double maximumTemperature = -std::numeric_limits<double>::infinity();
                for (int month = 0; month < kMonthsPerYear; ++month)
                {
                    const double monthlyMean =
                        meanMonthlyInsolation[static_cast<std::size_t>(month)];
                    const double normalizedMonthlyInsolation = monthlyMean > 0.0
                        ? monthlyInsolation[static_cast<std::size_t>(latitude)]
                                           [static_cast<std::size_t>(month)] /
                              monthlyMean
                        : 0.0;
                    const double seasonalAnomaly = std::clamp(
                        35.0 *
                            (normalizedMonthlyInsolation -
                             annualInsolation[static_cast<std::size_t>(latitude)]) *
                            thermalDamping,
                        -18.0,
                        18.0);
                    const double temperature = baseTemperatureC + seasonalAnomaly;
                    cell.monthlyTemperatureCelsius[static_cast<std::size_t>(month)] =
                        temperature;
                    meanTemperature += temperature / kMonthsPerYear;
                    minimumTemperature = std::min(minimumTemperature, temperature);
                    maximumTemperature = std::max(maximumTemperature, temperature);
                }
                cell.temperatureCelsius = meanTemperature;
                cell.temperatureAnnualRangeC =
                    maximumTemperature - minimumTemperature;
            }
            ReportRow(latitude, latB);
        }

        BeginLoadStep(m_progress, LoadStep::kClimatePrecipitation, "Simulating climate: precipitation");
        const double rotationScale = std::clamp(24.0 / rot, 0.4, 2.0);
        for (int latitude = 0; latitude < latB; ++latitude)
        {
            for (int longitude = 0; longitude < lonS; ++longitude)
            {
                auto& cell = Cell(latitude, longitude);
                const double circulation =
                    CirculationWetness(std::abs(cell.latitudeDegrees), rot);
                const double continentalDryness =
                    1.0 - cell.continentality * 0.6;
                const double moistureNoise =
                    SphericalFbm(
                        cell.latitudeDegrees,
                        cell.longitudeDegrees,
                        seed ^ 0x1234abcd,
                        5,
                        5);
                const double moistureBase = std::clamp(
                    circulation * std::sqrt(circulation) *
                        continentalDryness * (0.5 + moistureNoise * 0.5),
                    0.0,
                    1.0);

                double annualPrecipitation = 0.0;
                double annualTemperatureSaturation = 0.0;
                std::array<double, kMonthsPerYear> saturationFactors{};
                std::array<double, kMonthsPerYear> seasonalWeights{};
                for (int month = 0; month < kMonthsPerYear; ++month)
                {
                    const double temperature =
                        cell.monthlyTemperatureCelsius[static_cast<std::size_t>(month)];
                    saturationFactors[static_cast<std::size_t>(month)] =
                        std::clamp(std::exp((temperature - 15.0) * 0.055), 0.15, 2.5);
                    annualTemperatureSaturation +=
                        saturationFactors[static_cast<std::size_t>(month)] /
                        kMonthsPerYear;
                    const double seasonalLongitude =
                        2.0 * std::numbers::pi *
                        (static_cast<double>(month) + 0.5) / kMonthsPerYear -
                        std::numbers::pi / 2.0;
                    const double declinationDegrees =
                        std::asin(std::sin(tilt * std::numbers::pi / 180.0) *
                                  std::sin(seasonalLongitude)) *
                        180.0 / std::numbers::pi;
                    seasonalWeights[static_cast<std::size_t>(month)] =
                        SeasonalWetness(
                            cell.latitudeDegrees, declinationDegrees, rot);
                }

                const double saturationBaseline =
                    std::clamp(
                        std::exp((cell.temperatureCelsius - 15.0) * 0.055),
                        0.15,
                        2.5);
                annualPrecipitation =
                    moistureBase * 3200.0 * pressureScale * saturationBaseline;

                const double latitudeRadians =
                    cell.latitudeDegrees * std::numbers::pi / 180.0;
                const double cellWidthKm =
                    111.0 * std::max(0.12, std::cos(latitudeRadians));
                const int fetchSteps = std::clamp(
                    static_cast<int>(std::ceil(1800.0 / cellWidthKm)),
                    1,
                    std::max(1, lonS / 2));
                double windwardFactorSum = 0.0;
                const double seasonalWeightMean =
                    std::accumulate(
                        seasonalWeights.begin(),
                        seasonalWeights.end(),
                        0.0) /
                    kMonthsPerYear;

                for (int month = 0; month < kMonthsPerYear; ++month)
                {
                    const double seasonalLongitude =
                        2.0 * std::numbers::pi *
                        (static_cast<double>(month) + 0.5) / kMonthsPerYear -
                        std::numbers::pi / 2.0;
                    const double declinationDegrees =
                        std::asin(std::sin(tilt * std::numbers::pi / 180.0) *
                                  std::sin(seasonalLongitude)) *
                        180.0 / std::numbers::pi;
                    const double relativeLatitude =
                        std::abs(cell.latitudeDegrees - declinationDegrees);
                    const bool eastwardWind =
                        relativeLatitude > 30.0 / rotationScale &&
                        relativeLatitude < 60.0 / rotationScale;
                    const int windDirection = eastwardWind ? 1 : -1;
                    double oceanFetch = cell.isOcean ? 1.0 : 0.35;
                    double landFetchKm = 0.0;
                    if (!cell.isOcean)
                    {
                        for (int step = 1; step <= fetchSteps; ++step)
                        {
                            const int upwindLongitude =
                                (longitude - windDirection * step + lonS * 2) % lonS;
                            if (Cell(latitude, upwindLongitude).isOcean)
                            {
                                landFetchKm = (step - 1) * cellWidthKm;
                                oceanFetch =
                                    0.35 + 0.65 * std::exp(-landFetchKm / 2200.0);
                                break;
                            }
                            landFetchKm += cellWidthKm;
                        }
                    }

                    double upslopeKm = 0.0;
                    double upwindHighlandKm = 0.0;
                    if (!cell.isOcean)
                    {
                        const int immediateUpwind =
                            (longitude - windDirection + lonS * 2) % lonS;
                        upslopeKm = (cell.elevationNormalized -
                            Cell(latitude, immediateUpwind).elevationNormalized) *
                            kMaxElevationKm;
                        for (int step = 2; step <= 8; ++step)
                        {
                            const int upwindLongitude =
                                (longitude - windDirection * step + lonS * 2) % lonS;
                            upwindHighlandKm = std::max(
                                upwindHighlandKm,
                                (Cell(latitude, upwindLongitude).elevationNormalized -
                                 cell.elevationNormalized) *
                                    kMaxElevationKm);
                        }
                    }

                    const double uplift = std::clamp(upslopeKm / 1.2, 0.0, 0.75);
                    const double leeDrying =
                        std::exp(-std::max(0.0, upwindHighlandKm) * 0.16);
                    const double orographicFactor = std::clamp(
                        oceanFetch * (1.0 + uplift) * leeDrying,
                        0.25,
                        1.8);
                    windwardFactorSum += orographicFactor / kMonthsPerYear;

                    const double seasonalWeight =
                        seasonalWeightMean > 0.0
                            ? seasonalWeights[static_cast<std::size_t>(month)] /
                                  seasonalWeightMean
                            : 1.0;
                    const double monthlySaturation =
                        saturationFactors[static_cast<std::size_t>(month)];
                    const double saturationRatio =
                        annualTemperatureSaturation > 0.0
                            ? monthlySaturation / annualTemperatureSaturation
                            : 1.0;
                    cell.monthlyPrecipitationMm[static_cast<std::size_t>(month)] =
                        annualPrecipitation * seasonalWeight * saturationRatio *
                        orographicFactor / kMonthsPerYear;
                }

                cell.humidityNormalized =
                    std::clamp(moistureBase, 0.0, 1.0);
                cell.windMoistureFactor = windwardFactorSum;
                cell.precipitationMm = std::accumulate(
                    cell.monthlyPrecipitationMm.begin(),
                    cell.monthlyPrecipitationMm.end(),
                    0.0);
                cell.drySeasonMonths = static_cast<double>(std::count_if(
                    cell.monthlyPrecipitationMm.begin(),
                    cell.monthlyPrecipitationMm.end(),
                    [](double precipitation) { return precipitation < 30.0; }));

                const double potentialEvapotranspiration =
                    AnnualEvapotranspirationMm(cell.temperatureCelsius);
                cell.soilMoisture = std::clamp(
                    cell.precipitationMm /
                        (cell.precipitationMm + potentialEvapotranspiration + 1.0),
                    0.0,
                    1.0);
                cell.snowDays = 0.0;
                for (int month = 0; month < kMonthsPerYear; ++month)
                {
                    const double temperature =
                        cell.monthlyTemperatureCelsius[static_cast<std::size_t>(month)];
                    if (temperature < 0.0)
                    {
                        const double coldFraction =
                            std::clamp(-temperature / 10.0, 0.0, 1.0);
                        const double snowFraction = std::clamp(
                            cell.monthlyPrecipitationMm[static_cast<std::size_t>(month)] /
                                80.0,
                            0.0,
                            1.0);
                        cell.snowDays += 30.0 * coldFraction * snowFraction;
                    }
                }
            }
            ReportRow(latitude, latB);
        }

    }
}
