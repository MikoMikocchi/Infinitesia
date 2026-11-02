#include "planet/PlanetMap.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia
{
    namespace
    {
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

        double SmoothStep(double value)
        {
            return value * value * (3.0 - 2.0 * value);
        }

        double ValueNoise2D(
            double latitude,
            double longitude,
            int latitudeCells,
            int longitudeCells,
            std::uint32_t seed)
        {
            const double latitudePosition = std::clamp(latitude, 0.0, 1.0) * latitudeCells;
            const double longitudePosition =
                (longitude - std::floor(longitude)) * longitudeCells;
            const int latitude0 = std::min(static_cast<int>(latitudePosition), latitudeCells);
            const int latitude1 = std::min(latitude0 + 1, latitudeCells);
            const int longitude0 = static_cast<int>(longitudePosition) % longitudeCells;
            const int longitude1 = (longitude0 + 1) % longitudeCells;
            const double latitudeBlend = SmoothStep(latitudePosition - latitude0);
            const double longitudeBlend = SmoothStep(longitudePosition - std::floor(longitudePosition));

            const auto sample = [seed](int latitudeCell, int longitudeCell)
            {
                const std::uint32_t value =
                    seed ^
                    (static_cast<std::uint32_t>(latitudeCell + 1) * 0x9e3779b9u) ^
                    (static_cast<std::uint32_t>(longitudeCell + 1) * 0x85ebca6bu);
                return HashToUnit(value);
            };

            const double northWest = sample(latitude0, longitude0);
            const double northEast = sample(latitude0, longitude1);
            const double southWest = sample(latitude1, longitude0);
            const double southEast = sample(latitude1, longitude1);
            const double north = northWest + (northEast - northWest) * longitudeBlend;
            const double south = southWest + (southEast - southWest) * longitudeBlend;
            return north + (south - north) * latitudeBlend;
        }

        double SmoothNoise2D(
            int latitudeIndex,
            int longitudeIndex,
            int latitudeBands,
            int longitudeSegments,
            std::uint32_t seed)
        {
            const double latitude = (static_cast<double>(latitudeIndex) + 0.5) / latitudeBands;
            const double longitude = (static_cast<double>(longitudeIndex) + 0.5) / longitudeSegments;
            const double broadFeatures = ValueNoise2D(latitude, longitude, 8, 16, seed);
            const double regionalFeatures =
                ValueNoise2D(latitude, longitude, 16, 32, seed ^ 0x12345678u);
            const double localFeatures =
                ValueNoise2D(latitude, longitude, 32, 64, seed ^ 0x9abcdef0u);
            return broadFeatures * 0.65 + regionalFeatures * 0.25 + localFeatures * 0.10;
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

        const double structuredNoise = SmoothNoise2D(
            normalizedLatitude,
            normalizedLongitude,
            latitudeBands,
            longitudeSegments,
            m_planet.GetSeed());

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
