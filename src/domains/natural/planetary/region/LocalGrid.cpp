#include "domains/natural/planetary/region/LocalGrid.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia
{
    LocalGrid::LocalGrid(double planetRadiusMeters, CartesianPoint3D anchorDirection)
        : m_radiusMeters(planetRadiusMeters)
        , m_frame(FrameAt(anchorDirection))
    {
    }

    bool LocalGrid::IsValid(RegionId id) noexcept
    {
        return std::abs(id.i) <= kMaxRegionIndex && std::abs(id.j) <= kMaxRegionIndex;
    }

    TangentFrame LocalGrid::FrameAt(CartesianPoint3D direction) noexcept
    {
        const auto normalize = [](CartesianPoint3D v)
        {
            const double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            return CartesianPoint3D{ v.x / length, v.y / length, v.z / length };
        };

        const CartesianPoint3D center = normalize(direction);
        const auto horizontal = [&center](CartesianPoint3D axis)
        {
            const double along = axis.x * center.x + axis.y * center.y + axis.z * center.z;
            return CartesianPoint3D{ axis.x - along * center.x, axis.y - along * center.y, axis.z - along * center.z };
        };

        CartesianPoint3D north = horizontal({ 0.0, 1.0, 0.0 });
        if (north.x * north.x + north.y * north.y + north.z * north.z < 1e-12)
        {
            // Exactly on a pole every direction is south or north; pick a fixed one.
            north = horizontal({ 0.0, 0.0, 1.0 });
        }
        north = normalize(north);

        return {
            center,
            normalize({ center.y * north.z - center.z * north.y,
                        center.z * north.x - center.x * north.z,
                        center.x * north.y - center.y * north.x }),
            north
        };
    }

    CartesianPoint3D LocalGrid::Direction(double eastMeters, double northMeters) const noexcept
    {
        const double e = eastMeters / m_radiusMeters;
        const double n = northMeters / m_radiusMeters;
        const double x = m_frame.center.x + m_frame.east.x * e + m_frame.north.x * n;
        const double y = m_frame.center.y + m_frame.east.y * e + m_frame.north.y * n;
        const double z = m_frame.center.z + m_frame.east.z * e + m_frame.north.z * n;
        const double length = std::sqrt(x * x + y * y + z * z);
        return { x / length, y / length, z / length };
    }

    void LocalGrid::Project(CartesianPoint3D direction, double& eastMeters, double& northMeters) const noexcept
    {
        // Intersect the ray with the tangent plane at the anchor, then read off the axes.
        const double depth = direction.x * m_frame.center.x + direction.y * m_frame.center.y +
                             direction.z * m_frame.center.z;
        const double scale = m_radiusMeters / std::max(depth, 1e-9);
        eastMeters = (direction.x * m_frame.east.x + direction.y * m_frame.east.y + direction.z * m_frame.east.z) * scale;
        northMeters = (direction.x * m_frame.north.x + direction.y * m_frame.north.y + direction.z * m_frame.north.z) * scale;
    }

    RegionId LocalGrid::RegionAt(CartesianPoint3D direction) const noexcept
    {
        double east = 0.0;
        double north = 0.0;
        Project(direction, east, north);
        return {
            static_cast<int>(std::floor(east / kRegionSizeMeters + 0.5)),
            static_cast<int>(std::floor(north / kRegionSizeMeters + 0.5))
        };
    }

    CartesianPoint3D LocalGrid::PointDirection(RegionId id, double x, double y) const noexcept
    {
        return Direction((id.i - 0.5) * kRegionSizeMeters + x * kCellSizeMeters,
                         (id.j - 0.5) * kRegionSizeMeters + y * kCellSizeMeters);
    }

    CartesianPoint3D LocalGrid::CenterDirection(RegionId id) const noexcept
    {
        return Direction(id.i * kRegionSizeMeters, id.j * kRegionSizeMeters);
    }

    GeographicCoordinates LocalGrid::Center(RegionId id) const noexcept
    {
        return PlanetaryCoordinates::ToGeographic(CenterDirection(id));
    }
}
