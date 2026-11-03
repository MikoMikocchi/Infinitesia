#pragma once

#include "coordinates/PlanetaryCoordinates.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace Infinitesia
{
    // Address of one region inside a LocalGrid: signed grid coordinates, east and north
    // of the anchor region (0, 0). Not to be confused with PlanetRegion, which is a
    // single cell of the 55 km planet grid.
    struct RegionId
    {
        int i = 0; // east
        int j = 0; // north

        friend bool operator==(const RegionId&, const RegionId&) = default;

        std::uint64_t Pack() const noexcept
        {
            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(i)) << 32) |
                   static_cast<std::uint64_t>(static_cast<std::uint32_t>(j));
        }
    };

    // Local north-up frame at a point: unit vectors in PlanetaryCoordinates space.
    // PlanetaryCoordinates is mirrored relative to the real sphere (longitude grows
    // clockwise seen from the north pole, the globe mesh compensates with -longitude),
    // so east = center x north is what reads as "right" on a map seen from outside.
    struct TangentFrame
    {
        CartesianPoint3D center{};
        CartesianPoint3D east{};
        CartesianPoint3D north{};
    };

    // One chessboard of identical squares laid over the planet around an anchor point:
    // 32 m cells, 128 x 128 cells (4096 m) per region, aligned with east/north at the
    // anchor. A game has one settlement and so one anchor, chosen anywhere on the
    // planet, poles included; the grid itself has no seams and no forbidden places.
    //
    // Grid coordinates are metres east and north of the anchor measured on the tangent
    // plane. They map to the sphere by projecting the plane point radially (gnomonic),
    // so horizontal distances stretch by about (d / R)^2: 0.1% at 200 km, 1% at 640 km,
    // 3% at 1600 km. Heights are measured from sea level, so the local world has no
    // visible curvature.
    //
    // The anchor is the centre of region (0, 0).
    class LocalGrid
    {
    public:
        static constexpr int kCellsPerRegionEdge = 128;
        static constexpr int kPointsPerRegionEdge = kCellsPerRegionEdge + 1;
        static constexpr double kCellSizeMeters = 32.0;
        static constexpr double kRegionSizeMeters = kCellsPerRegionEdge * kCellSizeMeters;

        // Regions farther than this from the anchor (about 1600 km) are refused: the
        // plane-to-sphere projection stretches too much there.
        static constexpr int kMaxRegionIndex = 400;

        LocalGrid() = default;
        LocalGrid(double planetRadiusMeters, CartesianPoint3D anchorDirection);

        double PlanetRadiusMeters() const noexcept { return m_radiusMeters; }
        const TangentFrame& Frame() const noexcept { return m_frame; }

        static bool IsValid(RegionId id) noexcept;

        // Unit direction at `eastMeters` / `northMeters` of the anchor, and the inverse.
        CartesianPoint3D Direction(double eastMeters, double northMeters) const noexcept;
        void Project(CartesianPoint3D direction, double& eastMeters, double& northMeters) const noexcept;

        RegionId RegionAt(CartesianPoint3D direction) const noexcept;

        // Direction of lattice point (x, y), in cells from the region's south-west
        // corner. Values outside [0, kCellsPerRegionEdge] continue into the neighbours.
        // Neighbouring regions share their border points exactly, on all four sides.
        CartesianPoint3D PointDirection(RegionId id, double x, double y) const noexcept;
        CartesianPoint3D CenterDirection(RegionId id) const noexcept;
        GeographicCoordinates Center(RegionId id) const noexcept;

        static TangentFrame FrameAt(CartesianPoint3D direction) noexcept;

    private:
        double m_radiusMeters = 1.0;
        TangentFrame m_frame{ { 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 1.0, 0.0 } };
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
