#pragma once

#include "planet/PlanetMap.h"

#include <cstdint>
#include <utility>

namespace Infinitesia::GenerationDetail
{
    struct Vec3d
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    struct EulerPole
    {
        Vec3d axis{};
        double omegaRadPerMy = 0.0;
    };

    struct PlateKinematics
    {
        EulerPole euler{};
    };

    struct TectonicPlate
    {
        int id = 0;
        double centerLat = 0.0;
        double centerLon = 0.0;
        bool isOceanic = false;
        double crustDensity = 2900.0;
        double crustThicknessKm = 7.0;
        PlateKinematics kinematics{};
    };

    enum class BoundaryType
    {
        Divergent,
        Convergent,
        Transform
    };

    enum class BoundarySubtype
    {
        MidOceanRidge,
        ContinentalRift,
        OceanicSubduction,
        ContinentalSubduction,
        ContinentalCollision,
        TransformFault
    };

    double RandUnit(std::uint32_t seed, std::uint32_t tag);
    double RandUnit(std::uint32_t seed, int latitude, int longitude);
    double RandRange(std::uint32_t seed, std::uint32_t tag, double minimum, double maximum);
    double SmootherStep(double value);
    double Lerp(double first, double second, double amount);
    double fBmGrid(
        int latitudeIndex,
        int longitudeIndex,
        int latitudeBands,
        int longitudeSegments,
        std::uint32_t seed,
        int baseLatitudeCells,
        int baseLongitudeCells,
        int octaves);

    Vec3d LatLonToCart(double latitudeDegrees, double longitudeDegrees);
    double SphericalFbm(
        double latitudeDegrees,
        double longitudeDegrees,
        std::uint32_t seed,
        int baseCells,
        int octaves);
    double GCDistance(
        double latitudeA,
        double longitudeA,
        double latitudeB,
        double longitudeB);
    std::pair<double, double> EulerVelocityDegPerMy(
        const EulerPole& pole,
        double latitudeDegrees,
        double longitudeDegrees);
    BoundaryType ClassifyBoundaryFromVelocity(
        double velocityALatitude,
        double velocityALongitude,
        double velocityBLatitude,
        double velocityBLongitude,
        double normalLatitude,
        double normalLongitude);
    BoundarySubtype DetailedBoundary(
        BoundaryType boundaryType,
        bool plateAOceanic,
        bool plateBOceanic);

    extern const double kOceanicRho;
    extern const double kContinentalRho;
    double AiryIsostasyKm(double crustThicknessKm, double crustDensity);
    double OceanicBathymetryKm(double crustAgeNormalized, double maximumAgeMyr);

    double InsolationFraction(double latitudeDegrees, double axialTiltDegrees);
    double SeasonalAmplitude(
        double latitudeDegrees,
        double axialTiltDegrees,
        double rotationHours);
    double EquilibriumTempK(double incidentFlux);
    double GreenhouseK(double pressureKPa);
    double LapseK(double elevationNormalized);
    double CirculationWetness(double absoluteLatitudeDegrees, double rotationHours);
    double DrySeasonMonths(double precipitationMm, double circulationWetness);
    double SoilFertility(
        RockType rock,
        double temperatureCelsius,
        double precipitationMm,
        double planetAgeFraction);
    double SoilDepth(
        RockType rock,
        double precipitationMm,
        double slope,
        double planetAgeFraction);
    Biome ClassifyBiome(const PlanetRegion& region);
}
