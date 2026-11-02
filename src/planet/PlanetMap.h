#pragma once

#include "planet/Planet.h"

#include <cstdint>

namespace Infinitesia
{
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
        bool isOcean = false;
    };

    class PlanetMap
    {
    public:
        explicit PlanetMap(PlanetParameters parameters = {});

        const Planet& GetPlanet() const noexcept;
        const PlanetParameters& GetParameters() const noexcept;
        int GetLatitudeBandCount() const noexcept;
        int GetLongitudeSegmentCount() const noexcept;

        PlanetRegion GetRegion(int latitudeIndex, int longitudeIndex) const;
        double SampleElevation(int latitudeIndex, int longitudeIndex) const;
        bool IsOcean(int latitudeIndex, int longitudeIndex) const;

    private:
        Planet m_planet;
    };
}
