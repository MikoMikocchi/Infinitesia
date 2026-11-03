#pragma once

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/geology/Province.h"

#include <cmath>
#include <cstdint>
#include <numbers>

// Shared by the planet-geology test files.
namespace InfinitesiaTests
{
    using namespace Infinitesia;

    inline constexpr double kEarthRadiusKm = 6371.0;

    inline double CellArea(const PlanetRegion& region)
    {
        return std::cos(region.latitudeDegrees * std::numbers::pi / 180.0);
    }

    // The default 0.5 degree planet, built once and shared.
    inline const PlanetMap& SharedMap()
    {
        static const PlanetMap map;
        return map;
    }

    // A coarser planet per seed, for statistics over several worlds.
    inline PlanetMap CoarseMap(std::uint32_t seed)
    {
        PlanetParameters parameters;
        parameters.latitudeBands = 180;
        parameters.longitudeSegments = 360;
        parameters.seed = seed;
        return PlanetMap(parameters);
    }

    inline bool IsOceanicType(ProvinceType type)
    {
        return type == ProvinceType::OceanicRidge || type == ProvinceType::AbyssalPlain ||
               type == ProvinceType::Trench || type == ProvinceType::IslandArc;
    }
}
