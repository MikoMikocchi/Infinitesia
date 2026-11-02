#pragma once

#include <cstdint>

namespace Infinitesia
{
    struct PlanetParameters
    {
        double radiusMeters = 6371000.0;
        double gravityMetersPerSecondSquared = 9.81;
        double rotationHours = 24.0;
        double atmospherePressureKPa = 101.325;
        std::uint32_t seed = 1;
        int latitudeBands = 180;
        int longitudeSegments = 360;

        bool IsValid() const
        {
            return radiusMeters > 0.0 &&
                   gravityMetersPerSecondSquared > 0.0 &&
                   rotationHours > 0.0 &&
                   atmospherePressureKPa > 0.0 &&
                   latitudeBands > 0 &&
                   longitudeSegments > 0;
        }
    };
}
