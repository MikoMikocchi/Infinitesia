#pragma once

#include "coordinates/PlanetaryCoordinates.h"
#include "planet/region/CubeSphere.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace Infinitesia
{
    // Address of one region: a square patch of a cube face. Not to be confused
    // with PlanetRegion, which is a single cell of the 55 km planet grid.
    struct RegionId
    {
        int face = 0;
        int i = 0; // along the face u axis
        int j = 0; // along the face v axis

        friend bool operator==(const RegionId&, const RegionId&) = default;

        std::uint64_t Pack() const noexcept
        {
            return (static_cast<std::uint64_t>(face) << 56) |
                   (static_cast<std::uint64_t>(static_cast<std::uint32_t>(i)) << 28) |
                   static_cast<std::uint64_t>(static_cast<std::uint32_t>(j));
        }
    };

    // Splits every cube face into regionsPerEdge x regionsPerEdge regions of
    // roughly 4 km, each carrying a lattice of 128 x 128 sample cells (~32 m).
    // Lattice points are addressed globally per face, so neighbouring regions
    // share their border points exactly.
    class RegionLayout
    {
    public:
        static constexpr int kCellsPerRegionEdge = 128;
        static constexpr int kPointsPerRegionEdge = kCellsPerRegionEdge + 1;
        static constexpr double kTargetRegionSizeMeters = 4096.0;

        explicit RegionLayout(double planetRadiusMeters);

        double PlanetRadiusMeters() const noexcept { return m_radiusMeters; }
        int RegionsPerEdge() const noexcept { return m_regionsPerEdge; }
        double RegionSizeMeters() const noexcept { return m_regionSizeMeters; }
        double CellSizeMeters() const noexcept { return m_regionSizeMeters / kCellsPerRegionEdge; }

        bool IsValid(RegionId id) const noexcept;
        RegionId RegionAt(CartesianPoint3D direction) const noexcept;

        // Unit direction of lattice point (x, y), measured in cells from the region's
        // origin corner. Values outside [0, kCellsPerRegionEdge] continue onto the
        // neighbouring area in the region's own face frame.
        CartesianPoint3D PointDirection(RegionId id, double x, double y) const noexcept;
        CartesianPoint3D CenterDirection(RegionId id) const noexcept;
        GeographicCoordinates Center(RegionId id) const noexcept;

        // Neighbour across an edge (dx, dy in {-1, 0, 1}); crosses cube edges.
        RegionId Neighbor(RegionId id, int dx, int dy) const noexcept;

    private:
        double m_radiusMeters;
        int m_regionsPerEdge;
        double m_regionSizeMeters;
    };
}

template <>
struct std::hash<Infinitesia::RegionId>
{
    std::size_t operator()(const Infinitesia::RegionId& id) const noexcept
    {
        return std::hash<std::uint64_t>{}(id.Pack());
    }
};
