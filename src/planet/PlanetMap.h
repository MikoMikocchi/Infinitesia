#pragma once

#include "planet/Planet.h"

#include <cstdint>

namespace Infinitesia
{
    // Biome classification used for rendering hints.
    enum class Biome : std::uint8_t
    {
        Ocean,
        DeepOcean,
        IceCap,
        Tundra,
        Boreal,
        Temperate,
        Grassland,
        Shrubland,
        Desert,
        Savanna,
        TropicalRainforest,
        AlpineRock,
        Glacier,
    };

    struct PlanetRegion
    {
        int latitudeIndex = 0;
        int longitudeIndex = 0;
        std::uint32_t seed = 0;
        double latitudeDegrees = 0.0;
        double longitudeDegrees = 0.0;
        double elevationNormalized = 0.0;
        double temperatureCelsius = 0.0;
        double precipitationMm = 0.0;
        double humidityNormalized = 0.0;    // 0..1
        bool isOcean = false;
        Biome biome = Biome::Ocean;
    };

    // Subset of StarSystem data that drives climate calculations.
    struct ClimateInputs
    {
        double meanIncidentFluxInEarthUnits = 1.0;
        double axialTiltDegrees = 23.44;
        double atmospherePressureKPa = 101.325;
        double volatileMassFraction = 0.02;
        double gravityMetersPerSecondSquared = 9.81;
    };

    class PlanetMap
    {
    public:
        explicit PlanetMap(PlanetParameters parameters = {}, ClimateInputs climate = {});

        const Planet& GetPlanet() const noexcept;
        const PlanetParameters& GetParameters() const noexcept;
        const ClimateInputs& GetClimateInputs() const noexcept;
        int GetLatitudeBandCount() const noexcept;
        int GetLongitudeSegmentCount() const noexcept;

        PlanetRegion GetRegion(int latitudeIndex, int longitudeIndex) const;
        double SampleElevation(int latitudeIndex, int longitudeIndex) const;
        bool IsOcean(int latitudeIndex, int longitudeIndex) const;

    private:
        Planet m_planet;
        ClimateInputs m_climate;
    };
}
