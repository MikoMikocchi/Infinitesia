#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace Infinitesia
{
    struct GeographicCoordinates
    {
        double latitudeDegrees = 0.0;
        double longitudeDegrees = 0.0;
    };

    struct CartesianPoint3D
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    class PlanetaryCoordinates
    {
    public:
        static CartesianPoint3D ToCartesian(
            GeographicCoordinates coordinates,
            double radius = 1.0)
        {
            const double latitudeRadians = coordinates.latitudeDegrees * std::numbers::pi / 180.0;
            const double longitudeRadians = coordinates.longitudeDegrees * std::numbers::pi / 180.0;
            const double cosLatitude = std::cos(latitudeRadians);

            return {
                radius * cosLatitude * std::cos(longitudeRadians),
                radius * std::sin(latitudeRadians),
                radius * cosLatitude * std::sin(longitudeRadians)
            };
        }

        static GeographicCoordinates ToGeographic(
            CartesianPoint3D point,
            double radius = 1.0)
        {
            const double length = std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
            if (length <= 0.0)
            {
                return { 0.0, 0.0 };
            }

            const double normalizedX = point.x / length;
            const double normalizedY = point.y / length;
            const double normalizedZ = point.z / length;

            const double latitudeRadians = std::asin(std::clamp(normalizedY, -1.0, 1.0));
            const double longitudeRadians = std::atan2(normalizedZ, normalizedX);

            GeographicCoordinates coordinates;
            coordinates.latitudeDegrees = latitudeRadians * 180.0 / std::numbers::pi;
            coordinates.longitudeDegrees = longitudeRadians * 180.0 / std::numbers::pi;
            if (coordinates.longitudeDegrees < 0.0)
            {
                coordinates.longitudeDegrees += 360.0;
            }

            return coordinates;
        }

        static std::uint32_t HashRegion(int latitudeIndex, int longitudeIndex, std::uint32_t seed)
        {
            std::uint32_t hash = seed ^ 0x9E3779B9u;
            hash ^= static_cast<std::uint32_t>(latitudeIndex + 1) * 0x85EBCA6Bu;
            hash ^= static_cast<std::uint32_t>(longitudeIndex + 1) * 0xC2B2AE3Du;
            hash ^= hash >> 16;
            hash *= 0x85EBCA6Bu;
            hash ^= hash >> 13;
            hash *= 0xC2B2AE3Du;
            hash ^= hash >> 16;
            return hash;
        }
    };
}
