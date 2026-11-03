#pragma once

#include "planet/PlanetMap.h"
#include "planet/region/RegionLayout.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Infinitesia
{
    // Refined fields for one region on a (kPointsPerRegionEdge)^2 lattice.
    // Row-major, index = y * kPointsPerRegionEdge + x. Border points are shared
    // with the neighbouring regions and hold identical values.
    struct RegionData
    {
        static constexpr int kPoints = RegionLayout::kPointsPerRegionEdge;

        RegionId id{};
        GeographicCoordinates center{};
        double cellSizeMeters = 0.0;
        std::vector<float> elevationMeters;
        std::vector<Biome> biome;
        float minElevationMeters = 0.0f;
        float maxElevationMeters = 0.0f;

        static std::size_t Index(int x, int y) noexcept
        {
            return static_cast<std::size_t>(y) * kPoints + static_cast<std::size_t>(x);
        }
        float ElevationAt(int x, int y) const noexcept { return elevationMeters[Index(x, y)]; }
        Biome BiomeAt(int x, int y) const noexcept { return biome[Index(x, y)]; }
    };

    // Lattice covering (2 * radius + 1)^2 regions around a centre region, in the
    // centre region's face frame. Unlike stitching neighbouring RegionData it stays
    // consistent across cube-face edges, where neighbours' axes are rotated.
    // Lattice point (x, y) sits at offset (x - radius * kCells, y - radius * kCells)
    // cells from the centre region's origin point; y grows toward the face v axis.
    struct RegionPatch
    {
        RegionId center{};
        int radiusRegions = 0;
        int points = 0; // points per edge
        double cellSizeMeters = 0.0;
        std::vector<float> elevationMeters;
        std::vector<Biome> biome;
        std::vector<std::uint32_t> planetCell; // index into the planet grid, row-major

        std::size_t Index(int x, int y) const noexcept
        {
            return static_cast<std::size_t>(y) * points + static_cast<std::size_t>(x);
        }
        float ElevationAt(int x, int y) const noexcept { return elevationMeters[Index(x, y)]; }
    };
}
