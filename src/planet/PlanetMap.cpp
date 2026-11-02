#include "planet/PlanetMap.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia
{
    namespace
    {
        constexpr double kTwoPi = 6.28318530717958647692;

        std::uint32_t RotateLeft(std::uint32_t value, int bits)
        {
            return (value << bits) | (value >> (32 - bits));
        }

        std::uint32_t Hash32(std::uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x85ebca6bu;
            value ^= value >> 13;
            value *= 0xc2b2ae3du;
            value ^= value >> 16;
            return value;
        }

        double HashToUnit(std::uint32_t value)
        {
            return static_cast<double>(Hash32(value & 0xffffffffu)) / static_cast<double>(0xffffffffu);
        }

        double SmoothNoise2D(int latitudeIndex, int longitudeIndex, std::uint32_t seed)
        {
            const std::uint32_t base = std::uint32_t(latitudeIndex + 17) * 0x9e3779b9u +
                                       std::uint32_t(longitudeIndex + 13) * 0x85ebca6bu +
                                       seed;
            const double sampleA = HashToUnit(base ^ 0x12345678u);
            const double sampleB = HashToUnit(base ^ 0x9abcdef0u);
            const double sampleC = HashToUnit(base ^ 0x0fedcba9u);
            const double sampleD = HashToUnit(base ^ 0xdeadbeefu);
            return (sampleA + sampleB + sampleC + sampleD) / 4.0;
        }

        double ClampNormalized(double value)
        {
            return std::clamp(value, -1.0, 1.0);
        }
    }

    PlanetMap::PlanetMap(PlanetParameters parameters)
        : m_planet(std::move(parameters))
    {
    }

    const Planet& PlanetMap::GetPlanet() const noexcept
    {
        return m_planet;
    }

    const PlanetParameters& PlanetMap::GetParameters() const noexcept
    {
        return m_planet.GetParameters();
    }

    int PlanetMap::GetLatitudeBandCount() const noexcept
    {
        return m_planet.GetParameters().latitudeBands;
    }

    int PlanetMap::GetLongitudeSegmentCount() const noexcept
    {
        return m_planet.GetParameters().longitudeSegments;
    }

    PlanetRegion PlanetMap::GetRegion(int latitudeIndex, int longitudeIndex) const
    {
        const int latitudeBands = std::max(1, GetLatitudeBandCount());
        const int longitudeSegments = std::max(1, GetLongitudeSegmentCount());

        const int normalizedLatitude = std::clamp(latitudeIndex, 0, latitudeBands - 1);
        const int normalizedLongitude = std::clamp(longitudeIndex, 0, longitudeSegments - 1);

        const auto regionCenter = m_planet.GetRegionCenter(normalizedLatitude, normalizedLongitude);
        const std::uint32_t regionSeed = m_planet.DeriveRegionSeed(normalizedLatitude, normalizedLongitude);

        const double latitudeFactor = std::sin((regionCenter.latitudeDegrees / 90.0) * kTwoPi * 0.5);
        const double longitudeFactor = std::sin((regionCenter.longitudeDegrees / 360.0) * kTwoPi);
        const double baseNoise = SmoothNoise2D(normalizedLatitude, normalizedLongitude, regionSeed);
        const double structuredNoise = baseNoise * 0.75 + latitudeFactor * 0.15 + longitudeFactor * 0.10;

        const double elevationNormalized = ClampNormalized((structuredNoise - 0.5) * 2.0);
        const bool ocean = elevationNormalized < 0.12;
        const double temperatureCelsius = 25.0 - std::abs(regionCenter.latitudeDegrees) * 0.7 +
                                         (ocean ? -3.0 : 2.0);
        const double precipitationMm = ocean ? 600.0 + structuredNoise * 500.0 : 300.0 + structuredNoise * 650.0;

        PlanetRegion region;
        region.latitudeIndex = normalizedLatitude;
        region.longitudeIndex = normalizedLongitude;
        region.seed = regionSeed;
        region.latitudeDegrees = regionCenter.latitudeDegrees;
        region.longitudeDegrees = regionCenter.longitudeDegrees;
        region.elevationNormalized = elevationNormalized;
        region.temperatureCelsius = temperatureCelsius;
        region.precipitationMm = precipitationMm;
        region.isOcean = ocean;
        return region;
    }

    double PlanetMap::SampleElevation(int latitudeIndex, int longitudeIndex) const
    {
        return GetRegion(latitudeIndex, longitudeIndex).elevationNormalized;
    }

    bool PlanetMap::IsOcean(int latitudeIndex, int longitudeIndex) const
    {
        return GetRegion(latitudeIndex, longitudeIndex).isOcean;
    }
}
